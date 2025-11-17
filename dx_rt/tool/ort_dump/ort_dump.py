#
# Copyright (C) 2018- DEEPX Ltd.
# All rights reserved.
#
# This software is the property of DEEPX and is provided exclusively to customers 
# who are supplied with DEEPX NPU (Neural Processing Unit). 
# Unauthorized sharing or usage is strictly prohibited by law.
# 
# This file uses ONNX Runtime (MIT License) - Copyright (c) Microsoft Corporation.
#

import numpy as np
import onnxruntime as ort
import os
import argparse

def get_numpy_dtype(onnx_type_str):
    """ONNX 타입 문자열을 NumPy 데이터 타입으로 변환합니다."""
    type_map = {
        'tensor(float)': np.float32,
        'tensor(float32)': np.float32,
        'tensor(float16)': np.float16,
        'tensor(double)': np.float64,
        'tensor(int8)': np.int8,
        'tensor(uint8)': np.uint8,
        'tensor(int16)': np.int16,
        'tensor(uint16)': np.uint16,
        'tensor(int32)': np.int32,
        'tensor(uint32)': np.uint32,
        'tensor(int64)': np.int64,
        'tensor(uint64)': np.uint64,
        'tensor(bool)': np.bool_,
    }
    return type_map.get(onnx_type_str)

def run_multi_input_inference(onnx_path, input_path, output_path, batch_size=1):
    """
    ONNX 모델의 다중 입력을 자동 분석하고, 단일 바이너리 파일을 분할하여 추론을 수행합니다.
    """
    # 1. ONNX 런타임 세션 생성
    try:
        session = ort.InferenceSession(onnx_path)
        print(f"✅ ONNX 모델 '{os.path.basename(onnx_path)}'을 성공적으로 로드했습니다.")
    except Exception as e:
        print(f"❌ 오류: ONNX 모델을 로드할 수 없습니다: {e}")
        return

    # 2. 모델의 모든 입력 텐서 정보 자동 분석
    inputs_meta = session.get_inputs()
    model_inputs = []
    print("\nℹ️ 모델 입력 자동 분석 결과:")
    for i, meta in enumerate(inputs_meta):
        # shape의 동적 차원(None, -1)을 사용자가 지정한 batch_size로 대체
        shape = [dim if isinstance(dim, int) and dim > 0 else batch_size for dim in meta.shape]
        dtype = get_numpy_dtype(meta.type)
        if dtype is None:
            print(f"❌ 오류: 지원하지 않는 ONNX 타입입니다: {meta.type}")
            return
            
        # 각 입력에 필요한 바이트 크기 계산
        size_in_bytes = np.prod(shape) * np.dtype(dtype).itemsize
        
        model_inputs.append({
            'name': meta.name,
            'shape': shape,
            'dtype': dtype,
            'size_bytes': int(size_in_bytes)
        })
        print(f"  - 입력 #{i+1}: 이름='{meta.name}', Shape={shape}, Type={dtype.__name__}")

    # 3. 단일 바이너리 파일 읽기 및 분할
    try:
        full_input_bytes = open(input_path, 'rb').read()
        print(f"\n✅ 입력 파일 '{os.path.basename(input_path)}' ({len(full_input_bytes)} bytes) 로드 완료.")
    except FileNotFoundError:
        print(f"❌ 오류: 입력 파일 '{input_path}'을 찾을 수 없습니다.")
        return

    feed_dict = {}
    current_offset = 0
    print("\n🔪 입력 데이터 분할 및 텐서 생성:")
    for info in model_inputs:
        chunk_bytes = full_input_bytes[current_offset : current_offset + info['size_bytes']]
        
        if len(chunk_bytes) < info['size_bytes']:
            print(f"❌ 오류: 입력 파일 크기가 부족합니다. '{info['name']}' 텐서 처리 중단.")
            return

        # 바이트 청크를 NumPy 배열로 변환하고 reshape
        tensor = np.frombuffer(chunk_bytes, dtype=info['dtype']).reshape(info['shape'])
        feed_dict[info['name']] = tensor
        current_offset += info['size_bytes']
        print(f"  - '{info['name']}' 텐서 생성 완료 (shape: {tensor.shape})")

    if current_offset != len(full_input_bytes):
        print(f"⚠️ 경고: 입력 파일에 사용되지 않은 데이터가 {len(full_input_bytes) - current_offset} bytes 남았습니다.")

    # 4. 추론 실행
    print("\n🚀 추론을 시작합니다...")
    outputs = session.run(None, feed_dict)
    print("✅ 추론이 완료되었습니다.")
    
    # 5. 첫 번째 출력 결과를 바이트 단위로 파일에 저장
    #   (참고: 모델 출력이 여러 개인 경우, 필요에 따라 outputs[1], outputs[2] 등을 처리해야 함)
    output_tensor = outputs[0]
    print(f"   - 출력 텐서(0) 정보: Shape={output_tensor.shape}, Type={output_tensor.dtype}")
    try:
        output_tensor.tofile(output_path)
        print(f"\n💾 추론 결과(첫 번째 출력)가 '{os.path.basename(output_path)}' 파일로 저장되었습니다.")
    except Exception as e:
        print(f"❌ 오류: 출력 파일을 저장하는 중 문제가 발생했습니다: {e}")


if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description="다중 입력을 지원하는 ONNX 추론 스크립트. 입력을 자동 분석하고 단일 bin 파일을 분할하여 사용합니다.",
        formatter_class=argparse.RawTextHelpFormatter
    )
    parser.add_argument('-m', '--model', type=str, required=True, help="ONNX 모델 파일 경로.")
    parser.add_argument('-i', '--input', type=str, required=True, help="모든 입력 데이터가 순서대로 합쳐진 단일 바이너리 파일 경로.")
    parser.add_argument('-o', '--output', type=str, required=True, help="결과를 저장할 바이너리 파일 경로.")
    parser.add_argument('--batch_size', type=int, default=1, help="모델의 동적 입력 차원(배치 크기)을 지정합니다. (기본값: 1)")

    args = parser.parse_args()
    run_multi_input_inference(args.model, args.input, args.output, args.batch_size)