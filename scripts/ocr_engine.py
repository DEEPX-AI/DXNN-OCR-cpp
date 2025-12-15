"""
OCR Engine utilities and model management
Contains functions for creating and managing OCR engines
"""

from dx_engine import InferenceEngine as IE
from dx_engine import InferenceOption as IO


def make_det_engines(model_dirname):
    """
    Create detection engines for different aspect ratios and heights

    Args:
        model_dirname (str): Directory containing detection model files

    Returns:
        dict: Dictionary mapping aspect ratios to height-based model dictionaries

    Note:
        Creates models for aspect ratios from 5 to 25 in intervals of 10,
        and heights of 10, 20, and 30 pixels
    """

    io = IO().set_use_ort(True)
    det_model_map = {}

    for res in [640, 960]:
        if "mobile" in model_dirname:
            model_path = f"{model_dirname}/det_mobile_{res}.dxnn"
        else:
            model_path = f"{model_dirname}/det_v5_{res}.dxnn"
        det_model_map[res] = IE(model_path, io)

    return det_model_map


def make_rec_engines(model_dirname):
    """
    Create recognition engines for different aspect ratios and heights
    
    Args:
        model_dirname (str): Directory containing recognition model files
        
    Returns:
        dict: Dictionary mapping aspect ratios to height-based model dictionaries
        
    Note:
        Creates models for aspect ratios from 5 to 25 in intervals of 10,
        and heights of 10, 20, and 30 pixels
    """
    
    io = IO().set_use_ort(True)
    rec_model_map = {}

    for i in [3, 5, 10, 15, 25, 35]:
        if "mobile" in model_dirname:
            model_path = f"{model_dirname}/rec_mobile_ratio_{i}.dxnn"
        else:
            model_path = f"{model_dirname}/rec_v5_ratio_{i}.dxnn"
        rec_model_map[i] = IE(model_path, io)
    
    return rec_model_map


def create_ocr_models(use_doc_preprocessing=True, use_mobile=False):
    """
    Create detection, classification, and recognition models for v5
    
    Returns:
        tuple: (det_model, cls_model, rec_models)
            - det_model: Detection model
            - cls_model: Classification model  
            - rec_models: Dictionary of recognition models
    """
    dir_name = "engine/models/dxnn_optimized"
    if use_mobile:
        dir_name = "engine/models/dxnn_mobile_optimized"
    det_model_path = dir_name
    cls_model_path = f"{dir_name}/textline_ori.dxnn"  # 기존 DXNN 모델 (주석 처리)
    rec_model_dirname = dir_name
    rec_dict_dir = f"{rec_model_dirname}/ppocrv5_dict.txt"
    doc_ori_model_path = f"{dir_name}/doc_ori_fixed.dxnn"

    det_model = make_det_engines(det_model_path)
    cls_model = IE(cls_model_path, IO().set_use_ort(True))  # 기존 DXNN 로딩 (주석 처리)
    rec_models = make_rec_engines(rec_model_dirname)
    doc_ori_model = IE(doc_ori_model_path, IO().set_use_ort(True))

    doc_unwarping_model = None
    
    if use_doc_preprocessing:
        doc_unwarping_path = f"{dir_name}/UVDoc_pruned_p3.dxnn" 
        doc_unwarping_model = IE(doc_unwarping_path, IO().set_use_ort(True))
        
    return det_model, cls_model, rec_models, rec_dict_dir, doc_ori_model, doc_unwarping_model


def create_ocr_workers(num_workers=3, use_doc_preprocessing=True, use_doc_orientation=True, use_mobile=False):
    """
    Create multiple OCR worker instances for parallel processing
    PP-OCRv5 구조: det + rec + doc_ori + UVDoc (cls 모델이 doc_ori 역할)
    
    Args:
        num_workers (int): Number of worker instances to create
        use_doc_preprocessing (bool): Document unwarping 사용 여부 (UVDoc)
        use_doc_orientation (bool): Document orientation 사용 여부 (doc_ori)
        
    Returns:
        list: List of PaddleOcr worker instances with document preprocessing
    """
    from engine.paddleocr import PaddleOcr
    print("Creating OCR models...", num_workers, "use_unwarping:", use_doc_preprocessing, "use_doc_ori:", use_doc_orientation, "use_mobile:", use_mobile)
    det_model, cls_model, rec_models, rec_dict_dir, doc_ori_model, doc_unwarping_model = create_ocr_models(
        use_doc_preprocessing=use_doc_preprocessing, use_mobile=use_mobile
    )
    
    ocr_workers = [
        PaddleOcr(
            det_model=det_model,
            cls_model=cls_model,
            rec_models=rec_models,
            rec_dict_dir=rec_dict_dir,
            doc_ori_model=doc_ori_model,
            doc_unwarping_model=doc_unwarping_model,
            use_doc_preprocessing=use_doc_preprocessing,
            use_doc_orientation=use_doc_orientation
        ) for _ in range(num_workers)
    ]
    
    return ocr_workers


def create_async_ocr_pipeline(use_doc_preprocessing=True, use_doc_orientation=True):
    """
    Create an async OCR pipeline instance
    
    Args:
        use_doc_preprocessing (bool): Document unwarping 사용 여부 (UVDoc)
        use_doc_orientation (bool): Document orientation 사용 여부 (doc_ori)
        
    Returns:
        AsyncPipelineOCR: Async OCR pipeline instance
    """
    from engine.paddleocr import AsyncPipelineOCR
    
    print("Creating async OCR pipeline...")
    print(f"  - Document unwarping: {use_doc_preprocessing}")
    print(f"  - Document orientation: {use_doc_orientation}")
    
    det_models, cls_model, rec_models, rec_dict_dir, doc_ori_model, doc_unwarping_model = create_ocr_models(
        use_doc_preprocessing=use_doc_preprocessing
    )
    
    async_pipeline = AsyncPipelineOCR(
        det_models=det_models,
        cls_model=cls_model,
        rec_models=rec_models,
        rec_dict_dir=rec_dict_dir,
        doc_ori_model=doc_ori_model,
        doc_unwarping_model=doc_unwarping_model,
        use_doc_preprocessing=use_doc_preprocessing,
        use_doc_orientation=use_doc_orientation
    )
    
    print("Async OCR pipeline created successfully!")
    return async_pipeline
