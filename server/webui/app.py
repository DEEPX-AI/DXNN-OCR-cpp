import atexit
import base64
import io
import json
import logging
import os
import tempfile
import threading
import time
import uuid
import zipfile
from pathlib import Path

import gradio as gr
import requests
from PIL import Image

# ============ 日志配置 ============
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    datefmt='%Y-%m-%d %H:%M:%S'
)
logger = logging.getLogger(__name__)

# Get absolute path for static files
BASE_DIR = Path(__file__).parent.resolve()

# ============ API 配置 (修改为 DeepX OCR Server) ============
API_URL = os.environ.get("API_URL", "http://localhost:8080/ocr")
API_BASE = os.environ.get("API_BASE", "http://localhost:8080")
TOKEN = os.environ.get("API_TOKEN", "deepx_token")

TITLE = "DeepX OCR Server Demo"

TEMP_DIR = tempfile.TemporaryDirectory()
atexit.register(TEMP_DIR.cleanup)

# ============ 主题配置 (与原项目完全一致) ============
paddle_theme = gr.themes.Soft(
    font=(gr.themes.GoogleFont("Roboto"), "Open Sans", "Arial", "sans-serif"),
    font_mono=(gr.themes.GoogleFont("Fira Code"), "monospace"),
    primary_hue=gr.themes.Color(
        c50="#e8eafc",
        c100="#c5c9f7",
        c200="#a1a7f2",
        c300="#7d85ed",
        c400="#5963e8",
        c500="#2932e1",  # 主色调
        c600="#242bb4",
        c700="#1e2487",
        c800="#181d5a",
        c900="#12162d",
        c950="#0c0f1d",
    ),
)

MAX_NUM_PAGES = 100
TMP_DELETE_TIME = 900
THREAD_WAKEUP_TIME = 600

# ============ CSS 样式 (与原项目完全一致) ============
CSS = """
/* ===== Baidu AI Studio PaddleOCR Style CSS ===== */

/* ===== CSS Variables ===== */
:root {
    --primary-color: #2932E1;
    --primary-hover: #515eed;
    --primary-light: #e8eafc;
    
    /* ===== 强制所有 Gradio 组件使用浅色背景 ===== */
    --background-fill-primary: #ffffff !important;
    --background-fill-secondary: #F8F9FF !important;
    --block-background-fill: #ffffff !important;
    --block-label-background-fill: #F8F9FF !important;
    --button-secondary-background-fill: #ffffff !important;
    --button-secondary-background-fill-hover: var(--primary-light) !important;
    --button-primary-background-fill-hover: var(--primary-hover) !important;
    --input-background-fill: #ffffff !important;
    --table-odd-background-fill: #ffffff !important;
    --table-even-background-fill: #F8F9FF !important;
    --checkbox-background-color: #ffffff !important;
    --checkbox-background-color-hover: var(--primary-light) !important;
    --slider-color: var(--primary-color) !important;
    --title-color: #140E35;
    --text-color: #565772;
    --text-light: #9498AC;
    --text-disabled: #C8CEDE;
    --bg-main: #F8F9FB;
    --bg-white: #ffffff;
    --bg-hover: #F7F7F9;
    --bg-disabled: #f5f5f5;
    --border-color: #E8EDF6;
    --border-input: #d9d9d9;
    --shadow-card: 0 2px 8px rgba(37, 38, 94, 0.08);
    --shadow-hover: 0 4px 12px rgba(37, 38, 94, 0.12);
    --radius-sm: 4px;
    --radius-md: 8px;
    --radius-lg: 12px;
}

/* ===== Global Styles ===== */
body, .gradio-container {
    background-color: var(--bg-main) !important;
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif !important;
    color: var(--text-color) !important;
}

/* Global text color for all elements */
.gradio-container * {
    color: inherit;
}

/* Ensure all text elements have proper color */
p, span, div, li, td, th {
    color: var(--text-color);
}

/* Fix for any white text on light background */
.gr-box, .gr-panel, .white-container {
    color: var(--text-color) !important;
}

.gradio-container {
    max-width: 100% !important;
    width: 100% !important;
    margin: 0 !important;
    padding: 0 !important;
}

/* Force main containers to use full width */
.gradio-container .app,
.gradio-container main,
.gradio-container .wrap,
.gradio-container .contain {
    max-width: none !important;
    width: 100% !important;
}

/* ===== Typography ===== */
#markdown-title {
    text-align: center;
    color: var(--title-color) !important;
    font-weight: 600 !important;
    margin-bottom: 8px !important;
}

#markdown-title h1 {
    color: var(--primary-color) !important;
    font-size: 32px !important;
    font-weight: 700 !important;
}

label, .gr-label {
    color: var(--title-color) !important;
    font-size: 14px !important;
    font-weight: 400 !important;
    margin-bottom: 4px !important;
}

/* Remove block-info background */
span[data-testid="block-info"] {
    background: transparent !important;
    border: none !important;
    padding: 0 !important;
    color: inherit !important;
    font-weight: inherit !important;
}

/* Hide Examples default label */
.gallery.svelte-p5q82i {
    margin-top: 0 !important;
}

.block.svelte-1svsvh2 .label.svelte-p5q82i {
    display: none !important;
}

/* Custom Markdown Headers - All levels use same title color */
.custom-markdown h3,
.custom-markdown h4,
.custom-markdown h5,
.custom-markdown h6 {
    color: var(--title-color) !important;
    font-weight: 600 !important;
    margin-bottom: 16px !important;
}

.custom-markdown h3 {
    font-size: 20px !important;
}

.custom-markdown h4 {
    font-size: 16px !important;
}

.custom-markdown h5 {
    font-size: 14px !important;
    margin-bottom: 12px !important;
}

/* ===== Sidebar Toggle ===== */
#sidebar-toggle-btn {
    position: fixed !important;
    left: 0 !important;
    top: 50% !important;
    transform: translateY(-50%) !important;
    z-index: 1000 !important;
    background: linear-gradient(135deg, var(--primary-color) 0%, #4658FF 100%) !important;
    color: #FFFFFF !important;
    border: none !important;
    border-radius: 0 12px 12px 0 !important;
    padding: 18px 12px !important;
    cursor: pointer !important;
    box-shadow: 3px 0 12px rgba(41, 50, 225, 0.4) !important;
    transition: all 0.3s ease !important;
    font-size: 13px !important;
    font-weight: 700 !important;
    display: flex !important;
    flex-direction: column !important;
    align-items: center !important;
    gap: 8px !important;
    line-height: 1 !important;
    letter-spacing: 0.5px !important;
}

#sidebar-toggle-btn:hover {
    background: linear-gradient(135deg, #4658FF 0%, var(--primary-color) 100%) !important;
    box-shadow: 3px 0 16px rgba(41, 50, 225, 0.5) !important;
    padding-right: 16px !important;
}

#sidebar-toggle-btn .toggle-icon {
    font-size: 20px !important;
    display: block !important;
    color: #FFFFFF !important;
    font-weight: bold !important;
}

#sidebar-toggle-btn .toggle-text {
    font-size: 12px !important;
    display: block !important;
    white-space: nowrap !important;
    color: #FFFFFF !important;
    font-weight: 700 !important;
    text-shadow: 0 1px 2px rgba(0, 0, 0, 0.1) !important;
    writing-mode: vertical-rl !important;
    text-orientation: mixed !important;
}

.sidebar-column {
    transition: all 0.3s ease !important;
    overflow: visible !important;
    position: relative !important;
}

.sidebar-hidden {
    transform: translateX(-90%) !important;
    opacity: 0.3 !important;
    pointer-events: none !important;
}

.sidebar-hidden:hover {
    opacity: 0.5 !important;
}

/* ===== Card & Panel ===== */
.gr-panel, .gr-box, .gr-group {
    background: var(--bg-white) !important;
    border: 1px solid var(--border-color) !important;
    border-radius: var(--radius-lg) !important;
    box-shadow: var(--shadow-card) !important;
}

.form { background: transparent !important; }

/* ===== Buttons ===== */
#analyze-btn, #unzip-btn {
    color: white !important;
    border: none !important;
    border-radius: var(--radius-md) !important;
    padding: 12px 32px !important;
    font-size: 16px !important;
    font-weight: 500 !important;
    transition: all 0.3s ease !important;
}

#analyze-btn {
    background: linear-gradient(135deg, var(--primary-color) 0%, #4658FF 100%) !important;
    box-shadow: 0 4px 12px rgba(41, 50, 225, 0.25) !important;
}

#analyze-btn:hover {
    background: linear-gradient(135deg, #4658FF 0%, var(--primary-color) 100%) !important;
    transform: translateY(-2px) !important;
    box-shadow: 0 6px 16px rgba(41, 50, 225, 0.35) !important;
}

#unzip-btn {
    background: linear-gradient(135deg, #52c41a 0%, #73d13d 100%) !important;
    box-shadow: 0 4px 12px rgba(82, 196, 26, 0.25) !important;
}

#unzip-btn:hover {
    background: linear-gradient(135deg, #73d13d 0%, #52c41a 100%) !important;
    transform: translateY(-2px) !important;
    box-shadow: 0 6px 16px rgba(82, 196, 26, 0.35) !important;
}

/* Drag and Drop File Upload Area */
.upload-area {
    width: 100% !important;
}

.drag-drop-file {
    background: #FAFBFF !important;
    border: 1px dashed #D9D9D9 !important;
    border-radius: var(--radius-md) !important;
    transition: all 0.3s ease !important;
    color: var(--text-color) !important;
    cursor: pointer !important;
    width: 100% !important;
    font-size: 14px !important;
    text-align: center !important;
}

.drag-drop-file:active {
    background: #E8EAFF !important;
}

.drag-drop-file:hover {
    border-color: var(--primary-color) !important;
    background: #F0F2FF !important;
}

.drag-drop-file-custom {
    background: #FAFBFF !important;
    border: 1px dashed #D9D9D9 !important;
    border-radius: var(--radius-md) !important;
    transition: all 0.3s ease !important;
    color: var(--text-color) !important;
    cursor: pointer !important;
    width: 100% !important;
    font-size: 14px !important;
    text-align: center !important;
}

.drag-drop-file-custom button {
    min-height: 100px !important;
    height: 100px !important;
    padding: 0 !important;
    display: flex !important;
    align-items: center !important;
    justify-content: center !important;
    position: relative !important;
    flex-direction: column !important;
}

/* Hide original content */
.drag-drop-file-custom button .wrap {
    display: none !important;
}

/* Add custom content - only when no file */
.drag-drop-file-custom:not(:has(.file-preview-holder)) button::before {
    content: "📤" !important;
    display: block !important;
    font-size: 32px !important;
    margin-bottom: 8px !important;
}

.drag-drop-file-custom:not(:has(.file-preview-holder)) button::after {
    content: "Click or drag file to upload\\ASupport formats: PDF, JPG, PNG, JPEG" !important;
    white-space: pre-wrap !important;
    display: block !important;
    font-size: 13px !important;
    line-height: 1.6 !important;
    color: var(--text-color) !important;
    text-align: center !important;
}

/* When file is uploaded, show normal layout */
.drag-drop-file-custom:has(.file-preview-holder) {
    border-style: solid !important;
    min-height: auto !important;
}

.drag-drop-file-custom:has(.file-preview-holder) button {
    min-height: auto !important;
    height: auto !important;
    padding: 0 !important;
}

.drag-drop-file-custom:has(.file-preview-holder) button::before,
.drag-drop-file-custom:has(.file-preview-holder) button::after {
    display: none !important;
}

.drag-drop-file-custom:hover button::after {
    color: var(--primary-color) !important;
}

.drag-drop-file-custom:active {
    background: #E8EAFF !important;
}

.drag-drop-file-custom:hover {
    border-color: var(--primary-color) !important;
    background: #F0F2FF !important;
}

.drag-drop-file-custom label[data-testid="block-label"] {
    display: none !important;
}

.drag-drop-file-custom .upload-container {
    padding: 0 !important;
}

.drag-drop-file-custom .file-preview-holder {
    margin-top: 8px !important;
    background: #F0F2FF !important;
    border-radius: var(--radius-sm) !important;
}

/* 强制文件上传组件使用白色/浅色背景 */
.drag-drop-file-custom .file-preview,
.drag-drop-file-custom .file-preview *,
.drag-drop-file-custom [data-testid="file"],
.drag-drop-file-custom .wrap,
.drag-drop-file-custom button[aria-label],
.upload-area .file-preview,
.upload-area [data-testid="file"],
.upload-area .wrap {
    background: #ffffff !important;
    background-color: #ffffff !important;
}

/* 文件信息区域 - 浅蓝色背景 */
.drag-drop-file-custom .file-preview-holder,
.drag-drop-file-custom .file-preview-holder *,
.file-preview .name,
.file-preview .size {
    background: #F0F2FF !important;
    color: var(--title-color) !important;
}

/* 文件上传按钮和预览区域 */
.drag-drop-file-custom button,
.drag-drop-file-custom .upload-button {
    background: #FAFBFF !important;
}

/* 已上传文件的显示区域 */
.drag-drop-file-custom:has(.file-preview-holder) {
    background: #ffffff !important;
}

/* Gradio 文件组件内部样式覆盖 */
.gr-file .file-preview,
.gr-file .wrap,
.gr-file button {
    background: #ffffff !important;
    color: var(--title-color) !important;
}

/* 文件大小和名称文字颜色 */
.file-preview .file-name,
.file-preview .file-size,
.file-preview span {
    color: var(--title-color) !important;
    background: transparent !important;
    padding: 8px !important;
}

.file-status {
    margin-top: 8px !important;
    color: #52c41a !important;
    font-weight: 500 !important;
    background: #F0FFF0 !important;
    border: 1px solid #B7EB8F !important;
    border-radius: 6px !important;
    padding: 8px 12px !important;
}

/* 文件状态 Textbox 的内部元素 */
.file-status input,
.file-status textarea,
.file-status .wrap,
.file-status > div {
    background: transparent !important;
    color: #389E0D !important;
    border: none !important;
}

/* ===== Tabs ===== */
.tabs, .gr-tabs {
    background: var(--bg-white) !important;
    border-radius: var(--radius-lg) !important;
    padding: 16px !important;
    border: 1px solid var(--border-color) !important;
}

/* White Container (same style as Tabs) */
.white-container {
    background: var(--bg-white) !important;
    border-radius: var(--radius-lg) !important;
    padding: 16px !important;
    border: 1px solid var(--border-color) !important;
}

.tab-nav, .gr-tab-nav {
    background: var(--bg-hover) !important;
    border-radius: var(--radius-md) !important;
    padding: 4px !important;
    gap: 4px !important;
}

.tab-nav button, .gr-tab-nav button {
    background: transparent !important;
    color: var(--title-color) !important;
    border: none !important;
    border-radius: var(--radius-sm) !important;
    padding: 8px 16px !important;
    font-size: 14px !important;
    font-weight: 500 !important;
    transition: all 0.2s ease !important;
}

.tab-nav button.selected, .gr-tab-nav button.selected,
.tab-nav button[aria-selected="true"], .gr-tab-nav button[aria-selected="true"] {
    background: var(--primary-color) !important;
    color: #ffffff !important;
    font-weight: 600 !important;
    box-shadow: 0 1px 3px rgba(0, 0, 0, 0.1) !important;
}

.tab-nav button:hover, .gr-tab-nav button:hover {
    background: var(--primary-light) !important;
    color: var(--primary-color) !important;
}

/* ===== 强制所有按钮悬停状态使用浅色背景 ===== */
button:hover,
.gr-button:hover,
[role="tab"]:hover,
.svelte-1p9xokt:hover,
[data-testid]:hover,
.tab-nav button:hover,
.tabs button:hover {
    background-color: var(--primary-light) !important;
    color: var(--primary-color) !important;
}

/* 强制覆盖 Gradio 暗色悬停样式 */
*:hover {
    --block-background-fill: #ffffff;
    --button-secondary-background-fill-hover: var(--primary-light);
    --button-primary-background-fill-hover: var(--primary-hover);
}

/* ===== 全局强制浅色背景 ===== */
/* 所有按钮悬停状态 */
.gr-button:hover,
.secondary:hover,
button.secondary:hover,
[data-testid="button"]:hover,
.gr-box:hover,
.gr-input:hover,
.gr-check-radio:hover,
.svelte-1p9xokt:hover {
    background: var(--primary-light) !important;
    background-color: var(--primary-light) !important;
}

/* 强制所有输入框和区域使用白色背景 */
input, textarea, select,
.gr-box, .gr-input, .gr-form,
[data-testid], .svelte-1p9xokt,
.wrap, .container {
    background-color: #ffffff !important;
}

/* 强制禁用暗色模式 */
.dark, [data-theme="dark"], .dark-mode {
    --background-fill-primary: #ffffff !important;
    --background-fill-secondary: #F8F9FF !important;
    --block-background-fill: #ffffff !important;
}

/* Tab 按钮选中状态 - 使用主色调背景 */
button[role="tab"][aria-selected="true"],
[role="tab"].selected,
.tabs button[aria-selected="true"],
.gr-tabs button[aria-selected="true"] {
    background: var(--primary-color) !important;
    background-color: var(--primary-color) !important;
    color: #ffffff !important;
}

/* Tab 按钮悬停但未选中 */
button[role="tab"]:hover:not([aria-selected="true"]),
[role="tab"]:hover:not(.selected) {
    background: var(--primary-light) !important;
    background-color: var(--primary-light) !important;
    color: var(--primary-color) !important;
}

/* ===== Common Form Item Base Style ===== */
#use_doc_orientation_classify_cb,
#use_doc_unwarping_cb,
#use_textline_orientation_cb,
#text_det_thresh_nb,
#text_det_box_thresh_nb,
#text_det_unclip_ratio_nb,
#text_rec_score_thresh_nb,
#pdf_dpi_nb,
#pdf_max_pages_nb {
    padding: 8px 0 !important;
    background: transparent !important;
    border: none !important;
    border-width: 0 !important;
    border-radius: 0 !important;
    margin-bottom: 4px !important;
}

#use_doc_orientation_classify_cb:hover,
#use_doc_unwarping_cb:hover,
#use_textline_orientation_cb:hover,
#text_det_thresh_nb:hover,
#text_det_box_thresh_nb:hover,
#text_det_unclip_ratio_nb:hover,
#text_rec_score_thresh_nb:hover,
#pdf_dpi_nb:hover,
#pdf_max_pages_nb:hover {
    border-color: transparent !important;
    box-shadow: none !important;
}

/* ===== Common Label Style ===== */
#text_det_thresh_nb span[data-testid="block-info"],
#text_det_box_thresh_nb span[data-testid="block-info"],
#text_det_unclip_ratio_nb span[data-testid="block-info"],
#text_rec_score_thresh_nb span[data-testid="block-info"],
#pdf_dpi_nb span[data-testid="block-info"],
#pdf_max_pages_nb span[data-testid="block-info"] {
    font-size: 14px !important;
    font-weight: 400 !important;
    color: var(--title-color) !important;
}

/* ===== Toggle Switch Style (Module Tab) ===== */
#use_doc_orientation_classify_cb > label,
#use_doc_unwarping_cb > label,
#use_textline_orientation_cb > label {
    display: flex !important;
    flex-direction: row-reverse !important;
    align-items: center !important;
    justify-content: space-between !important;
    width: 100% !important;
    cursor: pointer !important;
    font-size: 14px !important;
    font-weight: 400 !important;
    color: var(--title-color) !important;
}

#use_doc_orientation_classify_cb input[type="checkbox"],
#use_doc_unwarping_cb input[type="checkbox"],
#use_textline_orientation_cb input[type="checkbox"] {
    width: 36px !important;
    height: 20px !important;
    appearance: none !important;
    -webkit-appearance: none !important;
    background: #bfbfbf !important;
    border-radius: 10px !important;
    position: relative !important;
    cursor: pointer !important;
    transition: all 0.3s ease !important;
    flex-shrink: 0 !important;
    margin: 0 !important;
}

#use_doc_orientation_classify_cb input[type="checkbox"]::before,
#use_doc_unwarping_cb input[type="checkbox"]::before,
#use_textline_orientation_cb input[type="checkbox"]::before {
    content: '' !important;
    position: absolute !important;
    width: 16px !important;
    height: 16px !important;
    background: white !important;
    border-radius: 50% !important;
    top: 2px !important;
    left: 2px !important;
    transition: all 0.3s ease !important;
    box-shadow: 0 2px 4px rgba(0, 0, 0, 0.15) !important;
}

#use_doc_orientation_classify_cb input[type="checkbox"]:checked,
#use_doc_unwarping_cb input[type="checkbox"]:checked,
#use_textline_orientation_cb input[type="checkbox"]:checked {
    background: var(--primary-color) !important;
}

#use_doc_orientation_classify_cb input[type="checkbox"]:checked::before,
#use_doc_unwarping_cb input[type="checkbox"]:checked::before,
#use_textline_orientation_cb input[type="checkbox"]:checked::before {
    left: 18px !important;
}

/* ===== Number Input Style ===== */
#text_det_thresh_nb > label,
#text_det_box_thresh_nb > label,
#text_det_unclip_ratio_nb > label,
#text_rec_score_thresh_nb > label,
#pdf_dpi_nb > label,
#pdf_max_pages_nb > label {
    display: flex !important;
    flex-direction: row !important;
    align-items: center !important;
    justify-content: space-between !important;
    gap: 12px !important;
}

#text_det_thresh_nb span[data-testid="block-info"],
#text_det_box_thresh_nb span[data-testid="block-info"],
#text_det_unclip_ratio_nb span[data-testid="block-info"],
#text_rec_score_thresh_nb span[data-testid="block-info"],
#pdf_dpi_nb span[data-testid="block-info"],
#pdf_max_pages_nb span[data-testid="block-info"] {
    flex: 1 !important;
}

#text_det_thresh_nb input,
#text_det_box_thresh_nb input,
#text_det_unclip_ratio_nb input,
#text_rec_score_thresh_nb input,
#pdf_dpi_nb input,
#pdf_max_pages_nb input {
    border: 1px solid var(--border-input) !important;
    border-radius: var(--radius-sm) !important;
    padding: 4px 8px !important;
    font-size: 12px !important;
    width: 70px !important;
    height: 24px !important;
    text-align: center !important;
    transition: all 0.2s ease !important;
    background: #fff !important;
    flex-shrink: 0 !important;
}

#text_det_thresh_nb input:focus,
#text_det_box_thresh_nb input:focus,
#text_det_unclip_ratio_nb input:focus,
#text_rec_score_thresh_nb input:focus,
#pdf_dpi_nb input:focus,
#pdf_max_pages_nb input:focus {
    border-color: var(--primary-color) !important;
    box-shadow: 0 0 0 2px rgba(41, 50, 225, 0.1) !important;
    outline: none !important;
}

/* ===== Loader ===== */
.loader {
    border: 4px solid var(--bg-hover);
    border-top: 4px solid var(--primary-color);
    border-radius: 50%;
    width: 48px;
    height: 48px;
    animation: spin 1s linear infinite;
    margin: 24px auto;
}

@keyframes spin {
    0% { transform: rotate(0deg); }
    100% { transform: rotate(360deg); }
}

.loader-container {
    text-align: center;
    margin: 24px 0;
    color: var(--text-color);
}

.loader-container-prepare {
    text-align: left;
    margin: 16px 0;
}

.loader-container-prepare > div {
    background: linear-gradient(135deg, #f8faff 0%, #f0f4ff 100%) !important;
    border: 1px solid var(--border-color) !important;
    border-left: 4px solid var(--primary-color) !important;
}

/* ===== Gallery ===== */
.gr-gallery, .gallery {
    border-radius: var(--radius-lg) !important;
    overflow: hidden !important;
}

.gradio-gallery-item:hover {
    background-color: transparent !important;
    filter: none !important;
    transform: none !important;
}

/* OCR Result Gallery - Vertical scrollable list with dynamic height */
.ocr-result-gallery-vertical {
    width: 100% !important;
    max-width: 100% !important;
    height: auto !important;
    min-height: unset !important;
    max-height: 600px !important;  /* Will be overridden by JS */
    overflow-y: auto !important;
    overflow-x: hidden !important;
    padding: 12px !important;
    background: var(--bg-white) !important;
    border: 1px solid var(--border-color) !important;
    border-radius: var(--radius-lg) !important;
    scrollbar-width: thin !important;
    scrollbar-color: var(--primary-color) var(--bg-hover) !important;
    display: flex !important;
    flex-direction: column !important;
    align-items: center !important;  /* Center images horizontally */
}

/* Force Gallery wrapper to fit content */
.ocr-result-gallery-vertical > .wrap,
.ocr-result-gallery-vertical > div {
    height: auto !important;
    min-height: unset !important;
}

.ocr-result-gallery-vertical::-webkit-scrollbar {
    width: 8px !important;
}

.ocr-result-gallery-vertical::-webkit-scrollbar-track {
    background: var(--bg-hover) !important;
    border-radius: 4px !important;
}

.ocr-result-gallery-vertical::-webkit-scrollbar-thumb {
    background: var(--primary-color) !important;
    border-radius: 4px !important;
}

.ocr-result-gallery-vertical::-webkit-scrollbar-thumb:hover {
    background: var(--primary-hover) !important;
}

.ocr-result-gallery-vertical .grid-wrap {
    height: auto !important;
    min-height: unset !important;
    max-height: none !important;
    overflow: visible !important;
    display: flex !important;
    flex-direction: column !important;
    gap: 16px !important;
}

.ocr-result-gallery-vertical .grid-container {
    display: flex !important;
    flex-direction: column !important;
    gap: 16px !important;
}

.ocr-result-gallery-vertical .thumbnail-item {
    width: 100% !important;
    max-width: 100% !important;
    border-radius: var(--radius-md) !important;
    border: 2px solid var(--border-color) !important;
    transition: all 0.2s ease !important;
    margin-bottom: 8px !important;
    display: flex !important;
    justify-content: center !important;
    align-items: center !important;
}

.ocr-result-gallery-vertical .thumbnail-item:hover {
    border-color: var(--primary-color) !important;
    box-shadow: 0 2px 8px rgba(41, 50, 225, 0.15) !important;
}

.ocr-result-gallery-vertical .thumbnail-item.selected {
    border-color: var(--primary-color) !important;
}

/* Gallery images - centered and scaled to fill container */
.ocr-result-gallery-vertical .thumbnail-item img {
    width: 100% !important;
    height: auto !important;
    object-fit: contain !important;
    max-width: 100% !important;
    margin: 0 auto !important;
}

/* JSON Output Scrollable Container - dynamic height */
#json-output-scrollable {
    max-height: 600px !important;  /* Will be overridden by JS */
    overflow-y: auto !important;
    overflow-x: hidden !important;
    scrollbar-width: thin !important;
    scrollbar-color: var(--primary-color) var(--bg-hover) !important;
    background: var(--bg-white) !important;
    border: 1px solid var(--border-color) !important;
    border-radius: var(--radius-lg) !important;
    padding: 12px !important;
    margin: 0 !important;
}

#json-output-scrollable::-webkit-scrollbar {
    width: 8px !important;
}

#json-output-scrollable::-webkit-scrollbar-track {
    background: var(--bg-hover) !important;
    border-radius: 4px !important;
}

#json-output-scrollable::-webkit-scrollbar-thumb {
    background: var(--primary-color) !important;
    border-radius: 4px !important;
}

#json-output-scrollable::-webkit-scrollbar-thumb:hover {
    background: var(--primary-hover) !important;
}

/* Results Content Wrapper - contains Tabs and Download button */
#results-content-wrapper {
    display: flex !important;
    flex-direction: column !important;
    height: auto !important;
}

/* Results content - OCR and JSON displayed together vertically */
#results-content-wrapper > .block,
#results-content-wrapper > div:not(.download-btn-container) {
    margin-bottom: 12px !important;
}

/* Download button container - fixed at Results bottom with minimal top margin */
.download-btn-container {
    margin-top: 4px !important;
    margin-bottom: 0 !important;
    padding: 0 !important;
    gap: 12px !important;
    flex-shrink: 0 !important;
}

.download-btn-container .download-file {
    margin: 0 !important;
}

/* OCR gallery - small bottom margin to Tab Content boundary */
#ocr-gallery-scrollable {
    margin: 0 !important;
    margin-bottom: 1px !important;
    height: auto !important;
    min-height: unset !important;
}

/* OCR gallery wrapper - fit content height */
#ocr-gallery-scrollable > div,
#ocr-gallery-scrollable .wrap,
#ocr-gallery-scrollable .svelte-1p9xokt {
    height: auto !important;
    min-height: unset !important;
}

/* OCR Gallery - remove all internal spacing and fixed heights */
.ocr-result-gallery-vertical .preview,
.ocr-result-gallery-vertical .caption-container,
.ocr-result-gallery-vertical > .wrap,
.ocr-result-gallery-vertical > div {
    margin: 0 !important;
    padding: 0 !important;
    height: auto !important;
    min-height: unset !important;
}

/* Remove any fixed height from all Gallery internals */
#ocr-gallery-scrollable,
#ocr-gallery-scrollable > *,
#ocr-gallery-scrollable > * > *,
.ocr-result-gallery-vertical,
.ocr-result-gallery-vertical > *,
.ocr-result-gallery-vertical > * > * {
    min-height: unset !important;
}

/* Force content to fit - remove any grid/flex related height constraints */
.ocr-result-gallery-vertical [class*="grid"],
.ocr-result-gallery-vertical [class*="wrap"],
.ocr-result-gallery-vertical [class*="container"] {
    height: auto !important;
    min-height: unset !important;
}

/* JSON container - small bottom margin to Tab Content boundary */
.json-output-container {
    margin: 0 !important;
    padding: 0 !important;
    margin-bottom: 1px !important;
}

/* ===== 初始状态隐藏控件 ===== */
/* 隐藏空的 Gallery 占位符 (裂图) */
#ocr-gallery-scrollable:empty,
#ocr-gallery-scrollable .grid-wrap:empty,
.ocr-result-gallery-vertical:empty {
    display: none !important;
}

/* 隐藏没有图片的 Gallery */
#ocr-gallery-scrollable:not(:has(img)),
.ocr-result-gallery-vertical:not(:has(img)) {
    display: none !important;
    visibility: hidden !important;
    height: 0 !important;
    min-height: 0 !important;
    padding: 0 !important;
    margin: 0 !important;
    border: none !important;
    overflow: hidden !important;
}

/* 隐藏空的 JSON 组件 */
#json-output-scrollable:empty,
#json-output-scrollable:not(:has(.json-holder)) {
    display: none !important;
}

/* 当父容器隐藏时，确保子组件也完全隐藏 */
#results-content-wrapper[style*="display: none"] * {
    display: none !important;
}

/* 隐藏空的下载按钮容器 */
.download-btn-container:empty {
    display: none !important;
}

/* ===== Spacing Classes ===== */
.tight-spacing { margin-bottom: -5px !important; }

.tight-spacing-as {
    margin-top: 8px !important;
    margin-bottom: 8px !important;
    padding: 12px 16px !important;
    background: var(--bg-white) !important;
    border-radius: var(--radius-md) !important;
    border-left: 3px solid var(--primary-color) !important;
    color: var(--text-color) !important;
    font-size: 14px !important;
    line-height: 1.6 !important;
}

.image-container img { display: inline-block !important; }

/* ===== File Download & JSON ===== */
.file-download { margin-top: 16px !important; }

.json-holder {
    background: var(--bg-white) !important;
    border-radius: var(--radius-md) !important;
    border: 1px solid var(--border-color) !important;
}

/* ===== JSON Viewer Text Color Fix ===== */
.json-holder span,
.json-holder .line,
.json-holder code,
[data-testid="json"] span,
[data-testid="json"] .line,
.gr-json span,
.gr-json .line {
    color: var(--text-color) !important;
}

/* JSON key names */
.json-holder .key,
[data-testid="json"] .key {
    color: var(--primary-color) !important;
}

/* JSON string values */
.json-holder .string,
[data-testid="json"] .string {
    color: #22863a !important;
}

/* JSON number values */
.json-holder .number,
[data-testid="json"] .number {
    color: #005cc5 !important;
}

/* ===== Examples Gallery Button Text Fix ===== */
.gallery button,
.gr-samples button,
.gr-examples button,
[data-testid="examples"] button {
    color: var(--text-color) !important;
}

.gallery button span,
.gr-samples button span,
.gr-examples button span {
    color: var(--text-color) !important;
}

/* Examples file names */
.gr-sample-textbox,
.sample-textbox,
[data-testid="textbox"] input {
    color: var(--text-color) !important;
}

/* ===== Layout ===== */
.main-row {
    display: grid !important;
    grid-template-columns: 360px 1fr !important;
    column-gap: 16px !important;
    align-items: stretch !important;  /* Keep both columns same height */
    width: 100% !important;
    margin: 0 !important;
}

.main-row > .column {
    min-width: 0 !important;
}

#sidebar-column {
    width: 360px !important;
    max-width: 360px !important;
    grid-column: 1 !important;
    position: sticky !important;
    top: 0 !important;
    align-self: stretch !important;
    overflow: visible !important;
}

#results-column {
    min-width: 0 !important;
    grid-column: 2 !important;
    align-self: stretch !important;  /* Match sidebar height */
}

/* Results column white container - fill height and match sidebar */
#results-column > .white-container {
    display: flex !important;
    flex-direction: column !important;
    height: 100% !important;
    min-height: inherit !important;
}

/* Results content wrapper - compact content at top, but can expand */
#results-content-wrapper {
    flex-shrink: 0 !important;
    flex-grow: 1 !important;  /* Allow expanding to fill remaining space */
    display: flex !important;
    flex-direction: column !important;
}

/* Results content - vertical layout */
#results-content-wrapper {
    display: flex !important;
    flex-direction: column !important;
}

/* ===== Responsive ===== */
@media (max-width: 1100px) {
    .main-row {
        grid-template-columns: 1fr !important;
        row-gap: 16px !important;
    }

    #sidebar-column,
    #results-column {
        grid-column: 1 !important;
        width: 100% !important;
        max-width: 100% !important;
        min-width: 0 !important;
        position: static !important;
        min-height: auto !important;
    }
}

@media (max-width: 768px) {
    .gradio-container { padding: 0 !important; }
    
    #analyze-btn, #unzip-btn {
        padding: 10px 20px !important;
        font-size: 14px !important;
    }
}

/* ===== Banner ===== */
.banner-container {
    background: transparent !important;
    margin: 0 !important;
    padding: 0 !important;
    width: 100% !important;
    max-width: 100% !important;
    border: none !important;
    box-shadow: none !important;
    border-radius: 0 !important;
}

.banner-container .image-container {
    background: transparent !important;
    display: flex !important;
    justify-content: center !important;
    align-items: center !important;
    width: 100% !important;
}

.banner-container .image-container button {
    cursor: default !important;
    background: transparent !important;
}

.banner-container .image-frame {
    background: transparent !important;
    display: flex !important;
    justify-content: center !important;
    width: 100% !important;
}

.banner-container img {
    max-width: 100% !important;
    width: 100% !important;
    height: auto !important;
    display: block !important;
    margin: 0 !important;
    object-fit: contain !important;
}

.banner-container .icon-button-wrapper,
.banner-container .icon-buttons,
.banner-container .top-panel {
    display: none !important;
}

/* ===== FORCE TEXT COLOR FIXES ===== */

/* Force all Tab buttons to have visible text */
button[role="tab"],
.tabs button,
.gr-tabs button,
[data-testid="tab-nav"] button,
.tabitem button,
div[class*="tab"] button {
    color: var(--title-color) !important;
}

button[role="tab"][aria-selected="true"] {
    background: var(--primary-color) !important;
    color: #ffffff !important;
}

button[role="tab"]:hover:not([aria-selected="true"]) {
    background: var(--primary-light) !important;
    color: var(--primary-color) !important;
}

/* Force Number input labels to be visible */
.gr-number label,
.gr-number span,
.gr-number input,
input[type="number"],
.gr-input label,
.gr-input span {
    color: var(--title-color) !important;
}

/* Force all form labels */
.gr-form label,
.gr-form span[data-testid="block-info"],
.gr-block label,
.gr-block span {
    color: var(--title-color) !important;
}

/* JSON component text */
.gr-json,
.gr-json *,
pre, code,
.json-container,
.json-container * {
    color: var(--text-color) !important;
}

/* Specific JSON styling */
.gr-json .key { color: var(--primary-color) !important; }
.gr-json .string { color: #22863a !important; }
.gr-json .number { color: #005cc5 !important; }
.gr-json .boolean { color: #d73a49 !important; }
.gr-json .null { color: #6a737d !important; }

/* Svelte component overrides */
[class*="svelte"] button,
[class*="svelte"] span,
[class*="svelte"] label,
[class*="svelte"] input {
    color: var(--title-color) !important;
}

/* Specific Gradio 5.x overrides */
.block span,
.block label,
.wrap span,
.wrap label {
    color: var(--title-color) !important;
}

/* Input fields text color */
input, textarea, select {
    color: var(--title-color) !important;
}

/* Placeholder text */
input::placeholder,
textarea::placeholder {
    color: var(--text-light) !important;
}

/* ===== 强制所有 Textbox 使用浅色背景 ===== */
.gr-textbox,
.gr-textbox input,
.gr-textbox textarea,
.gr-textbox .wrap,
[data-testid="textbox"],
[data-testid="textbox"] input,
[data-testid="textbox"] textarea {
    background: #ffffff !important;
    background-color: #ffffff !important;
}

/* 禁用状态的 Textbox 使用浅绿色背景 */
.gr-textbox.disabled,
.gr-textbox[disabled],
.gr-textbox input:disabled,
.gr-textbox textarea:disabled {
    background: #F0FFF0 !important;
    color: #389E0D !important;
}
"""

EXAMPLE_DIR = BASE_DIR / "examples"
EXAMPLE_PDF_DIR = BASE_DIR / "examples_pdf"


# Dynamically load example files from directories
def load_examples_from_dir(directory, extensions):
    """Load all files with specified extensions from directory"""
    examples = []
    if directory.exists() and directory.is_dir():
        for file_path in sorted(directory.iterdir()):
            if file_path.is_file() and file_path.suffix.lower() in extensions:
                examples.append([str(file_path)])
    return examples


# Load image examples (png, jpg, jpeg)
EXAMPLE_TEST = load_examples_from_dir(EXAMPLE_DIR, {'.png', '.jpg', '.jpeg'})

# Load PDF examples
EXAMPLE_PDF = load_examples_from_dir(EXAMPLE_PDF_DIR, {'.pdf'})

DESC_DICT = {
    "use_doc_orientation_classify": "Enable the document image orientation classification module. When enabled, you can correct distorted images, such as wrinkles, tilts, etc.",
    "use_doc_unwarping": "Enable the document unwarping module. When enabled, you can correct distorted images, such as wrinkles, tilts, etc.",
    "use_textline_orientation": "Enable the text line orientation classification module to support the distinction and correction of text lines of 0 degrees and 180 degrees.",
    "text_det_thresh_nb": "In the output probability map, only pixels with scores greater than the threshold are considered text pixels, and the value range is 0~1.",
    "text_det_box_thresh_nb": "When the average score of all pixels in the detection result border is greater than the threshold, the result will be considered as a text area, and the value range is 0 to 1. If missed detection occurs, this value can be appropriately lowered.",
    "text_det_unclip_ratio_nb": "Use this method to expand the text area. The larger the value, the larger the expanded area.",
    "text_rec_score_thresh_nb": "After text detection, the text box performs text recognition, and the text results with scores greater than the threshold will be retained. The value range is 0~1.",
    "pdf_dpi_nb": "PDF rendering DPI. Higher values produce clearer images but use more memory. Recommended: 72-300.",
    "pdf_max_pages_nb": "Maximum number of PDF pages to process. Pages beyond this limit will not be processed.",
}

tmp_time = {}
lock = threading.Lock()


# ============ 配置变更日志函数 ============
def log_checkbox_change(name: str):
    """返回一个记录 Checkbox 变更的函数"""
    def _log(value):
        status = "启用" if value else "禁用"
        logger.info(f"[Settings] {name}: {status}")
        return value
    return _log


def log_number_change(name: str, unit: str = ""):
    """返回一个记录 Number 变更的函数"""
    def _log(value):
        unit_str = f" {unit}" if unit else ""
        logger.info(f"[Settings] {name}: {value}{unit_str}")
        return value
    return _log


def gen_tooltip_radio(desc_dict):
    tooltip = {}
    for key, desc in desc_dict.items():
        suffixes = ["_cb", "_rb", "_md"]
        if key.endswith("_nb"):
            suffix = "_nb"
            suffixes = ["_nb", "_md"]
            key = key[: -len(suffix)]
        for suffix in suffixes:
            tooltip[f"{key}{suffix}"] = desc
    return tooltip


TOOLTIP_RADIO = gen_tooltip_radio(DESC_DICT)


def url_to_bytes(url, *, timeout=10):
    """Download image from URL"""
    resp = requests.get(url, timeout=timeout)
    resp.raise_for_status()
    return resp.content


def base64_to_bytes(base64_str):
    """Decode base64 string to bytes"""
    return base64.b64decode(base64_str)


def get_image_bytes(image_data):
    """Get image bytes from either URL or base64 string"""
    if image_data is None:
        return None
    
    # Check if it's a URL (starts with http:// or https://)
    if isinstance(image_data, str) and (image_data.startswith('http://') or image_data.startswith('https://')):
        return url_to_bytes(image_data)
    # Check if it's a relative URL (starts with /static/)
    elif isinstance(image_data, str) and image_data.startswith('/static/'):
        full_url = f"{API_BASE}{image_data}"
        return url_to_bytes(full_url)
    # Otherwise assume it's base64
    elif isinstance(image_data, str):
        return base64_to_bytes(image_data)
    else:
        return None


def bytes_to_image(image_bytes):
    return Image.open(io.BytesIO(image_bytes))


# ============ API 处理函数 (适配 DeepX OCR Server) ============
def process_file(
    file_path,
    image_input,
    use_doc_orientation_classify,
    use_doc_unwarping,
    use_textline_orientation,
    text_det_thresh,
    text_det_box_thresh,
    text_det_unclip_ratio,
    text_rec_score_thresh,
    pdf_dpi,
    pdf_max_pages,
):
    """Process uploaded file with DeepX OCR Server API"""
    try:
        if not file_path and not image_input:
            return None
        
        if file_path:
            if Path(file_path).suffix.lower() == ".pdf":
                file_type = 0  # PDF
            else:
                file_type = 1  # Image
        else:
            file_path = image_input
            file_type = 1  # Image
        
        # 记录处理开始和配置参数
        file_name = os.path.basename(file_path) if file_path else "Unknown"
        file_type_str = "PDF" if file_type == 0 else "Image"
        logger.info(f"[Process] 开始处理文件: {file_name} (类型: {file_type_str})")
        logger.info(f"[Process] 当前配置参数:")
        logger.info(f"  - Module Selection:")
        logger.info(f"      图像方向校正: {'启用' if use_doc_orientation_classify else '禁用'}")
        logger.info(f"      图像畸变校正: {'启用' if use_doc_unwarping else '禁用'}")
        logger.info(f"      文本行方向校正: {'启用' if use_textline_orientation else '禁用'}")
        logger.info(f"  - OCR Settings:")
        logger.info(f"      文本检测像素阈值: {text_det_thresh}")
        logger.info(f"      文本检测框阈值: {text_det_box_thresh}")
        logger.info(f"      扩展系数: {text_det_unclip_ratio}")
        logger.info(f"      文本识别分数阈值: {text_rec_score_thresh}")
        if file_type == 0:
            logger.info(f"  - PDF Settings:")
            logger.info(f"      PDF渲染DPI: {pdf_dpi}")
            logger.info(f"      PDF最大页数: {pdf_max_pages}")
        
        # Read file content
        with open(file_path, "rb") as f:
            file_bytes = f.read()

        # Call DeepX OCR Server API
        file_data = base64.b64encode(file_bytes).decode("ascii")
        headers = {
            "Authorization": f"token {TOKEN}",
            "Content-Type": "application/json",
        }

        request_body = {
            "file": file_data,
            "fileType": file_type,
            "visualize": True,
            "useDocOrientationClassify": use_doc_orientation_classify,
            "useDocUnwarping": use_doc_unwarping,
            "useTextlineOrientation": use_textline_orientation,
            "textDetThresh": text_det_thresh,
            "textDetBoxThresh": text_det_box_thresh,
            "textDetUnclipRatio": text_det_unclip_ratio,
            "textRecScoreThresh": text_rec_score_thresh,
        }
        
        # Add PDF-specific parameters
        if file_type == 0:
            request_body["pdfDpi"] = int(pdf_dpi)
            request_body["pdfMaxPages"] = int(pdf_max_pages)

        response = requests.post(
            API_URL,
            json=request_body,
            headers=headers,
            timeout=300,
        )
        
        try:
            response.raise_for_status()
        except requests.exceptions.RequestException as e:
            raise RuntimeError(f"API request failed: {response.text}") from e
        
        # Parse API response
        result = response.json()
        
        if result.get("errorCode", 0) != 0:
            raise gr.Error(f"OCR processing failed: {result.get('errorMsg', 'Unknown error')}")
        
        overall_ocr_res_images = []
        output_json = result.get("result", {})
        input_images = []
        
        # Handle Image response (fileType=1)
        if file_type == 1:
            ocr_results = output_json.get("ocrResults", [])
            ocr_image_url = output_json.get("ocrImage", "")
            
            # Get visualization image
            if ocr_image_url:
                ocr_image_bytes = get_image_bytes(ocr_image_url)
                if ocr_image_bytes:
                    overall_ocr_res_images.append(ocr_image_bytes)
            
            # Add original input image
            input_images.append(file_bytes)
        
        # Handle PDF response (fileType=0)
        else:
            pages = output_json.get("pages", [])
            for page in pages:
                page_index = page.get("pageIndex", 0)
                ocr_image_url = page.get("ocrImage", "")
                
                if ocr_image_url:
                    ocr_image_bytes = get_image_bytes(ocr_image_url)
                    if ocr_image_bytes:
                        overall_ocr_res_images.append(ocr_image_bytes)
                        input_images.append(ocr_image_bytes)

        # 记录处理完成
        logger.info(f"[Process] 处理完成: {file_name}")
        logger.info(f"  - 结果图片数量: {len(overall_ocr_res_images)}")
        logger.info(f"  - 输入图片数量: {len(input_images)}")
        
        return {
            "original_file": file_path,
            "file_type": "pdf" if file_type == 0 else "image",
            "overall_ocr_res_images": overall_ocr_res_images,
            "output_json": output_json,
            "input_images": input_images,
            "api_response": result,
        }

    except requests.exceptions.RequestException as e:
        logger.error(f"[Process] API请求失败: {str(e)}")
        raise gr.Error(f"API request failed: {str(e)}")
    except Exception as e:
        logger.error(f"[Process] 处理文件时发生错误: {str(e)}")
        raise gr.Error(f"Error processing file: {str(e)}")


def export_full_results(results):
    """Create ZIP file with all analysis results"""
    try:
        global tmp_time
        if not results:
            raise ValueError("No results to export")

        filename = Path(results["original_file"]).stem + f"_{uuid.uuid4().hex}.zip"
        zip_path = Path(TEMP_DIR.name, filename)

        with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as zipf:
            for i, img_bytes in enumerate(results["overall_ocr_res_images"]):
                zipf.writestr(f"overall_ocr_res_images/page_{i+1}.jpg", img_bytes)

            zipf.writestr(
                "output.json",
                json.dumps(results["output_json"], indent=2, ensure_ascii=False),
            )

            # Add API response
            api_response = results.get("api_response", {})
            zipf.writestr(
                "api_response.json",
                json.dumps(api_response, indent=2, ensure_ascii=False),
            )

            for i, img_bytes in enumerate(results["input_images"]):
                zipf.writestr(f"input_images/page_{i+1}.jpg", img_bytes)
        
        with lock:
            tmp_time[zip_path] = time.time()
        return str(zip_path)

    except Exception as e:
        raise gr.Error(f"Error creating ZIP file: {str(e)}")


def on_file_change_from_examples_image(file):
    return on_common_change(file, "examples_image")


def on_file_change_from_examples_pdf(file):
    return on_common_change(file, "examples_pdf")


def on_file_change_from_input_impl(file_select, self_input, ref_input, called_from):
    if file_select != '':
        if ref_input is not None:
            if self_input is not None:
                if self_input != ref_input:
                    return on_common_change(self_input, called_from)
                else:
                    return gr.Textbox(value=None, visible=False), gr.File(value=None), gr.Image(value=None)
            else:
                return gr.skip(), gr.skip(), gr.skip()
        else:
            if self_input is not None:
                if (file_select in os.path.basename(self_input)):
                    return gr.Textbox(value=None, visible=False), gr.File(value=None), gr.Image(value=None)
                else:
                    return on_common_change(self_input, called_from)
            else:
                if self_input is None:
                    return gr.Textbox(value=None, visible=False), gr.File(value=None), gr.Image(value=None)
                else:
                    return gr.skip(), gr.skip(), gr.skip()
    else:
        return gr.skip(), gr.skip(), gr.skip()


def on_file_change_from_file_input(file_select, self_input, ref_input):
    return on_file_change_from_input_impl(file_select, self_input, ref_input, "file_input")


def on_file_change_from_image_input(file_select, self_input, ref_input):
    return on_file_change_from_input_impl(file_select, self_input, ref_input, "image_input")


def on_common_change_impl(file):
    """Handle file input change and return status textbox"""
    if file is not None:
        try:
            filename = os.path.basename(file.name) if hasattr(file, 'name') else os.path.basename(str(file))
            return gr.Textbox(value=f"✅ Chosen file: {filename}", visible=True)
        except Exception:
            return gr.Textbox(value="✅ File selected", visible=True)
    return gr.Textbox(value=None, visible=False)


def on_common_change(file, called_from):
    """Handle file input change and return status textbox"""
    input_select = on_common_change_impl(file)

    if called_from == 'examples_image':
        file_input = gr.File(value=None)
        image_input = gr.skip()
    elif called_from == 'examples_pdf':
        file_input = gr.skip()
        image_input = gr.Image(value=None)
    elif called_from == 'file_input':
        file_input = gr.skip()
        image_input = gr.Image(value=None)
    elif called_from == 'image_input':
        file_input = gr.File(value=None)
        image_input = gr.skip()
    else:
        raise ValueError("Invalid called_from value")
    
    return input_select, file_input, image_input


def validate_file_input(file_path, image_input):
    """Validate file selection"""
    if not file_path and not image_input:
        gr.Warning("📁 Please select a file first before parsing.")


def toggle_spinner(file_path, image_input):
    """Show spinner when file is present, hide results content"""
    if not file_path and not image_input:
        return (
            gr.skip(),
            gr.skip(),
            gr.skip(),
            gr.skip(),
        )
    return (
        gr.Column(visible=True),   # loading_spinner: show
        gr.Column(visible=False),  # prepare_spinner: hide
        gr.File(visible=False),    # download_file: hide
        gr.Column(visible=False),  # results_content: hide
    )


def hide_spinner(results):
    """Hide spinner and show results content when results are available"""
    if results:
        return gr.Column(visible=False), gr.Column(visible=True)
    else:
        return gr.Column(visible=False), gr.skip()


def update_display(results):
    if not results:
        # 返回隐藏状态的控件 (JSON, Gallery, Download按钮行)
        return [gr.skip()] * (1 + len(gallery_list) + 1)
    
    # Prepare JSON output (显示在 JSON Tab 的可滚动区域) - 显示并更新值
    output_json = [gr.JSON(value=results["output_json"], visible=True)]
    
    # Prepare gallery images from OCR result images (显示在 OCR Tab 的可滚动 Gallery)
    gallery_images = []
    for img_data in results["overall_ocr_res_images"]:
        if isinstance(img_data, bytes):
            gallery_images.append(bytes_to_image(img_data))
        else:
            gallery_images.append(img_data)
    
    # Update OCR Gallery with the OCR result images - 显示并更新值
    gallery_list_imgs = [gr.Gallery(value=gallery_images, visible=True)]
    
    # Download 按钮行 - 显示
    download_btn_visible = [gr.Row(visible=True)]
    
    return output_json + gallery_list_imgs + download_btn_visible


def delete_file_periodically():
    global tmp_time
    while True:
        current_time = time.time()
        delete_tmp = []
        for filename, start_time in list(tmp_time.items()):
            if (current_time - start_time) >= TMP_DELETE_TIME:
                if os.path.exists(filename):
                    os.remove(filename)
                    delete_tmp.append(filename)
        for filename in delete_tmp:
            with lock:
                del tmp_time[filename]
        time.sleep(THREAD_WAKEUP_TIME)


# Banner paths
BANNER_PATH = str(BASE_DIR / "res" / "img" / "deepx-baidu-pp-banner.png")
BANNER_CES_PATH = str(BASE_DIR / "res" / "img" / "DEEPX-Banner-CES-2026-01.png")

# Force English language script
FORCE_EN_SCRIPT = """
<script>
    try {
        Object.defineProperty(navigator, 'language', {
            get: function() { return 'en-US'; }
        });
        Object.defineProperty(navigator, 'languages', {
            get: function() { return ['en-US', 'en']; }
        });
    } catch (e) {
        console.log("Language override failed");
    }
</script>
"""

# ============ 构建 Gradio 界面 (与原项目布局完全一致) ============
with gr.Blocks(css=CSS, title=TITLE, theme=paddle_theme, head=FORCE_EN_SCRIPT) as demo:
    results_state = gr.State()

    # Top banner (full-width, outside main layout)
    gr.Image(
        value=BANNER_PATH,
        show_label=False,
        show_download_button=False,
        show_fullscreen_button=False,
        container=False,
        elem_classes=["banner-container"],
    )
    
    with gr.Row(elem_classes=["main-row"]):
        with gr.Column(scale=3, elem_classes=["sidebar-column"], elem_id="sidebar-column"):
        
            # Upload section
            gr.Markdown("#### 📁 Input File", elem_classes="custom-markdown")
            with gr.Column(elem_classes=["white-container"]):
                with gr.Column(elem_classes=["upload-area"]):
                    file_input = gr.File(
                        file_types=[".pdf", ".jpg", ".jpeg", ".png"],
                        type="filepath",
                        visible=True,
                        show_label=False,
                        elem_classes=["drag-drop-file-custom"],
                    )

                    file_select = gr.Textbox(
                        show_label=False, 
                        visible=False,
                        interactive=False,
                        elem_classes=["file-status"],
                    )

                process_btn = gr.Button(
                    "🚀 Parse Document", elem_id="analyze-btn", variant="primary"
                )

                gr.Markdown("##### 📷 Image Examples", elem_classes="custom-markdown")

                image_input = gr.Image(
                    label="Image",
                    sources="upload",
                    type="filepath",
                    visible=False,
                    interactive=True,
                    placeholder="Click to upload file",
                )

                examples_image = gr.Examples(
                    fn=on_file_change_from_examples_image,
                    inputs=image_input,
                    outputs=[file_select, file_input, image_input],
                    examples_per_page=8,
                    examples=EXAMPLE_TEST,
                    run_on_click=True,
                )
                
                gr.Markdown("##### 📄 PDF Examples", elem_classes="custom-markdown")
                examples_pdf = gr.Examples(
                    fn=on_file_change_from_examples_pdf,
                    inputs=file_input,
                    outputs=[file_select, file_input, image_input],
                    examples_per_page=5,
                    examples=EXAMPLE_PDF,
                    run_on_click=True,
                )

                image_input.change(
                    fn=on_file_change_from_image_input,
                    inputs=[file_select, image_input, file_input],
                    outputs=[file_select, file_input, image_input],
                )

                file_input.change(
                    fn=on_file_change_from_file_input, 
                    inputs=[file_select, file_input, image_input],
                    outputs=[file_select, file_input, image_input]
                )
            
            # Settings section
            gr.Markdown("#### ⚙️ Settings", elem_classes="custom-markdown")
            with gr.Tabs() as advance_options_tabs:
                with gr.Tab("Module Selection") as Module_Options:
                    use_doc_orientation_classify_cb = gr.Checkbox(
                        value=False,
                        interactive=True,
                        label="Image Orientation Correction",
                        show_label=True,
                        elem_id="use_doc_orientation_classify_cb",
                    )
                    use_doc_unwarping_cb = gr.Checkbox(
                        value=False,
                        interactive=True,
                        label="Image Distortion Correction",
                        show_label=True,
                        elem_id="use_doc_unwarping_cb",
                    )
                    use_textline_orientation_cb = gr.Checkbox(
                        value=False,
                        interactive=True,
                        label="Text Line Orientation Correction",
                        show_label=True,
                        elem_id="use_textline_orientation_cb",
                    )
                
                with gr.Tab("OCR Settings") as Text_detection_Options:
                    text_det_thresh_nb = gr.Number(
                        value=0.30,
                        step=0.01,
                        minimum=0.00,
                        maximum=1.00,
                        interactive=True,
                        label="Text Detection Pixel Threshold",
                        show_label=True,
                        elem_id="text_det_thresh_nb",
                    )
                    text_det_box_thresh_nb = gr.Number(
                        value=0.60,
                        step=0.01,
                        minimum=0.00,
                        maximum=1.00,
                        interactive=True,
                        label="Text Detection Box Threshold",
                        show_label=True,    
                        elem_id="text_det_box_thresh_nb",
                    )
                    text_det_unclip_ratio_nb = gr.Number(
                        value=1.5,
                        step=0.1,
                        minimum=1.0,
                        maximum=3.0,
                        interactive=True,
                        label="Expansion Coefficient",
                        show_label=True,
                        elem_id="text_det_unclip_ratio_nb",
                    )
                    text_rec_score_thresh_nb = gr.Number(
                        value=0.00,
                        step=0.01,
                        minimum=0,
                        maximum=1.00,
                        interactive=True,
                        label="Text Recognition Score Threshold",
                        show_label=True,
                        elem_id="text_rec_score_thresh_nb",
                    )

                with gr.Tab("PDF Settings") as PDF_Options:
                    pdf_dpi_nb = gr.Number(
                        value=150,
                        step=10,
                        minimum=72,
                        maximum=300,
                        interactive=True,
                        label="PDF Render DPI",
                        show_label=True,
                        elem_id="pdf_dpi_nb",
                    )
                    pdf_max_pages_nb = gr.Number(
                        value=10,
                        step=1,
                        minimum=1,
                        maximum=100,
                        interactive=True,
                        label="PDF Max Pages",
                        show_label=True,
                        elem_id="pdf_max_pages_nb",
                    )
                    gr.HTML(
                        """
                        <div style="
                            padding: 12px 16px;
                            background: #FFF7E6;
                            border-left: 3px solid #FAAD14;
                            border-radius: 6px;
                            margin-top: 12px;
                            font-size: 13px;
                            color: #8C6D1F;
                            line-height: 1.5;
                        ">
                            <strong style="color: #D48806;">⚠️ Memory Notice:</strong><br>
                            A4 page @ 150 DPI ≈ 8.7MB/page<br>
                            Recommended: DPI=150, Max Pages=10
                        </div>
                        """
                    )

        # Results display section
        with gr.Column(scale=7, elem_classes=["white-container"], elem_id="results-column"):

            gr.Markdown("### 📋 Results", elem_classes="custom-markdown")

            loading_spinner = gr.Column(
                visible=False, elem_classes=["loader-container"]
            )
            with loading_spinner:
                gr.HTML(
                    """
                    <div class="loader"></div>
                    <p style="color: #565772; font-size: 14px;">Processing, please wait...</p>
                    """
                )
            prepare_spinner = gr.Column(
                visible=True, elem_classes=["loader-container-prepare"]
            )
            with prepare_spinner:
                gr.HTML(
                    """
                    <div style="font-size: 18px; font-weight: 600; color: #140E35; margin-bottom: 16px; display: flex; align-items: center; gap: 8px;">
                        <span style="background: #2932E1; color: white; padding: 4px 10px; border-radius: 4px; font-size: 12px;">GUIDE</span>
                        User Guide
                    </div>
                    <div style="display: grid; gap: 12px; color: #565772;">
                        <div style="display: flex; align-items: flex-start; gap: 12px;">
                            <span style="background: #2932E1; color: white; width: 24px; height: 24px; border-radius: 50%; display: flex; align-items: center; justify-content: center; font-size: 12px; font-weight: 600; flex-shrink: 0;">1</span>
                            <div><b style="color: #140E35;">Upload Your File</b><br><span style="font-size: 13px; color: #565772;">Upload directly or select from Image/PDF Examples below<br>Supported formats: JPG, PNG, PDF, JPEG</span></div>
                        </div>
                        <div style="display: flex; align-items: flex-start; gap: 12px;">
                            <span style="background: #2932E1; color: white; width: 24px; height: 24px; border-radius: 50%; display: flex; align-items: center; justify-content: center; font-size: 12px; font-weight: 600; flex-shrink: 0;">2</span>
                            <div><b style="color: #140E35;">Click Parse Document Button</b><br><span style="font-size: 13px; color: #565772;">System will process automatically</span></div>
                        </div>
                        <div style="display: flex; align-items: flex-start; gap: 12px;">
                            <span style="background: #2932E1; color: white; width: 24px; height: 24px; border-radius: 50%; display: flex; align-items: center; justify-content: center; font-size: 12px; font-weight: 600; flex-shrink: 0;">3</span>
                            <div><b style="color: #140E35;">View & Download Results</b><br><span style="font-size: 13px; color: #565772;">Results will be displayed after processing</span></div>
                        </div>
                        <div style="display: flex; align-items: flex-start; gap: 12px;">
                            <span style="background: #2932E1; color: white; width: 24px; height: 24px; border-radius: 50%; display: flex; align-items: center; justify-content: center; font-size: 12px; font-weight: 600; flex-shrink: 0;">4</span>
                            <div><b style="color: #140E35;">Expand Results View</b><br><span style="font-size: 13px; color: #565772;">Click <b style="color: #140E35;">HIDE LEFT MENU</b> button on the left to view results in full screen</span></div>
                        </div>
                    </div>
                    <div style="margin-top: 16px; padding: 12px 16px; background: #F6FFED; border-radius: 6px; border-left: 3px solid #52C41A;">
                        <span style="font-weight: 600; color: #389E0D;">✅ Server Status:</span>
                        <span style="color: #237804;">DeepX OCR Server Ready</span>
                    </div>
                    """
                )

            output_json_list = []
            gallery_list = []
            
            # Results 内容区域 - 初始隐藏，只有在收到响应后才显示
            # OCR 图片和 JSON 一起显示，无需 Tab 切换
            with gr.Column(visible=False, elem_id="results-content-wrapper") as results_content:
                # OCR 识别结果图片：垂直滚动显示 - 初始隐藏
                gallery_ocr_det = gr.Gallery(
                    show_label=False,
                    allow_preview=False,  # 禁用双击放大预览功能
                    preview=False,
                    columns=1,
                    rows=None,
                    height="auto",
                    object_fit="contain",
                    elem_classes=["ocr-result-gallery-vertical"],
                    elem_id="ocr-gallery-scrollable",
                    visible=False,
                )
                gallery_list.append(gallery_ocr_det)
                
                # JSON 输出：可滚动的 JSON 显示控件 - 初始隐藏
                output_json_list.append(
                    gr.JSON(
                        visible=False,
                        elem_id="json-output-scrollable",
                        elem_classes=["json-output-container"],
                    )
                )
                
                # Download 按钮容器 - 初始隐藏
                with gr.Row(visible=False, elem_classes=["download-btn-container"]) as download_btn_row:
                    download_all_btn = gr.Button(
                        "📦 Download Full Results (ZIP)",
                        elem_id="unzip-btn",
                        variant="primary",
                    )
                    download_file = gr.File(visible=False, label="Download File", elem_classes=["download-file"])

    # Bottom banner (full-width, outside main layout)
    gr.Image(
        value=BANNER_CES_PATH,
        show_label=False,
        show_download_button=False,
        show_fullscreen_button=False,
        container=False,
        elem_classes=["banner-container"],
    )

    # Sidebar toggle button
    gr.HTML(
        """
        <button id="sidebar-toggle-btn">
            <span class="toggle-icon">◀</span>
            <span class="toggle-text">HIDE LEFT MENU</span>
        </button>
        """
    )

    gr.Markdown("", elem_classes="custom-markdown")
    
    # ============ 事件处理 ============
    process_btn.click(
        validate_file_input,
        inputs=[file_input, image_input],
        outputs=[],
    ).then(
        toggle_spinner,
        inputs=[file_input, image_input],
        outputs=[
            loading_spinner,
            prepare_spinner,
            download_file,
            results_content,
        ],
    ).then(
        process_file,
        inputs=[
            file_input,
            image_input,
            use_doc_orientation_classify_cb,
            use_doc_unwarping_cb,
            use_textline_orientation_cb,
            text_det_thresh_nb,
            text_det_box_thresh_nb,
            text_det_unclip_ratio_nb,
            text_rec_score_thresh_nb,
            pdf_dpi_nb,
            pdf_max_pages_nb,
        ],
        outputs=[results_state],
    ).then(
        hide_spinner, inputs=[results_state], outputs=[loading_spinner, results_content]
    ).then(
        update_display,
        inputs=[results_state],
        outputs=output_json_list + gallery_list + [download_btn_row],
    )

    # ============ 配置变更日志事件 ============
    # Module Selection 配置
    use_doc_orientation_classify_cb.change(
        fn=log_checkbox_change("Image Orientation Correction (图像方向校正)"),
        inputs=[use_doc_orientation_classify_cb],
        outputs=[use_doc_orientation_classify_cb],
    )
    use_doc_unwarping_cb.change(
        fn=log_checkbox_change("Image Distortion Correction (图像畸变校正)"),
        inputs=[use_doc_unwarping_cb],
        outputs=[use_doc_unwarping_cb],
    )
    use_textline_orientation_cb.change(
        fn=log_checkbox_change("Text Line Orientation Correction (文本行方向校正)"),
        inputs=[use_textline_orientation_cb],
        outputs=[use_textline_orientation_cb],
    )
    
    # OCR Settings 配置
    text_det_thresh_nb.change(
        fn=log_number_change("Text Detection Pixel Threshold (文本检测像素阈值)"),
        inputs=[text_det_thresh_nb],
        outputs=[text_det_thresh_nb],
    )
    text_det_box_thresh_nb.change(
        fn=log_number_change("Text Detection Box Threshold (文本检测框阈值)"),
        inputs=[text_det_box_thresh_nb],
        outputs=[text_det_box_thresh_nb],
    )
    text_det_unclip_ratio_nb.change(
        fn=log_number_change("Expansion Coefficient (扩展系数)"),
        inputs=[text_det_unclip_ratio_nb],
        outputs=[text_det_unclip_ratio_nb],
    )
    text_rec_score_thresh_nb.change(
        fn=log_number_change("Text Recognition Score Threshold (文本识别分数阈值)"),
        inputs=[text_rec_score_thresh_nb],
        outputs=[text_rec_score_thresh_nb],
    )
    
    # PDF Settings 配置
    pdf_dpi_nb.change(
        fn=log_number_change("PDF Render DPI (PDF渲染DPI)", "DPI"),
        inputs=[pdf_dpi_nb],
        outputs=[pdf_dpi_nb],
    )
    pdf_max_pages_nb.change(
        fn=log_number_change("PDF Max Pages (PDF最大页数)", "页"),
        inputs=[pdf_max_pages_nb],
        outputs=[pdf_max_pages_nb],
    )

    download_all_btn.click(
        export_full_results, inputs=[results_state], outputs=[download_file]
    ).success(
        fn=None,
        inputs=[],
        outputs=[],
        js="""
        () => {
            // 自动触发下载，无需显示额外的文件组件
            setTimeout(() => {
                const downloadFile = document.querySelector('.download-file');
                if (downloadFile) {
                    const downloadLink = downloadFile.querySelector('a[download]') || downloadFile.querySelector('a[href]');
                    if (downloadLink) {
                        downloadLink.click();
                    }
                }
            }, 100);
        }
        """
    )

    demo.load(
        fn=lambda: None,
        inputs=[],
        outputs=[],
        js=f"""
        () => {{
            // Sidebar toggle functionality
            let sidebarVisible = true;
            const toggleBtn = document.getElementById('sidebar-toggle-btn');
            const sidebar = document.getElementById('sidebar-column');
            const resultsColumn = document.getElementById('results-column');
            let __syncing = false;
            let __observer = null;
            // #region agent log
            const __agentLog = (hypothesisId, message, data) => {{
                fetch('http://localhost:7243/ingest/9ee15b4b-d4c5-4bd0-8443-e566ed3e8dee', {{
                    method: 'POST',
                    headers: {{ 'Content-Type': 'application/json' }},
                    body: JSON.stringify({{
                        sessionId: 'debug-session',
                        runId: 'align-debug',
                        hypothesisId,
                        location: 'app.py:1852',
                        message,
                        data,
                        timestamp: Date.now()
                    }})
                }}).catch(() => {{}});
            }};
            // #endregion

            // #region agent log
            const logAlignment = (reason) => {{
                if (!sidebar || !resultsColumn) return;
                const heading = resultsColumn.querySelector('h1, h2, h3, h4');
                const sidebarRect = sidebar.getBoundingClientRect();
                const resultsRect = resultsColumn.getBoundingClientRect();
                const headingRect = heading ? heading.getBoundingClientRect() : null;
                const resultsStyle = window.getComputedStyle(resultsColumn);
                __agentLog('H5', 'alignment:measure', {{
                    reason,
                    sidebarLeft: sidebarRect.left,
                    resultsLeft: resultsRect.left,
                    resultsPaddingLeft: resultsStyle.paddingLeft,
                    headingLeft: headingRect ? headingRect.left : null,
                    headingText: heading ? heading.textContent.trim().slice(0, 50) : null
                }});
            }};
            // #endregion

            function syncColumnHeights(reason) {{
                if (!sidebar || !resultsColumn) return;
                if (__syncing) return;
                __syncing = true;
                // #region agent log
                __agentLog('H1', 'syncColumnHeights:enter', {{
                    reason,
                    sidebarDisplay: sidebar.style.display,
                    sidebarH: sidebar.offsetHeight,
                    resultsH: resultsColumn.offsetHeight
                }});
                // #endregion
                if (__observer) __observer.disconnect();
                
                // Reset any previous height settings
                resultsColumn.style.minHeight = '';
                resultsColumn.style.height = '';
                
                // Get sidebar natural height (this is our target height)
                const sidebarHeight = sidebar.offsetHeight;
                
                // Get Results container (white-container inside results-column)
                const resultsContainer = resultsColumn.querySelector('.white-container') || resultsColumn;
                
                // Calculate fixed elements height in Results column
                const resultsTitle = resultsContainer.querySelector('h3, .custom-markdown');
                const downloadBtn = resultsContainer.querySelector('.download-btn-container');
                
                let fixedHeight = 32;  // Container padding (16px top + 16px bottom)
                if (resultsTitle) fixedHeight += resultsTitle.offsetHeight + 16;
                if (downloadBtn) fixedHeight += downloadBtn.offsetHeight + 8;
                
                // Calculate available height for scrollable content
                const scrollableMaxHeight = Math.max(300, sidebarHeight - fixedHeight);
                
                // Apply max-height to OCR Gallery and JSON container
                const ocrGallery = document.querySelector('.ocr-result-gallery-vertical');
                const jsonContainer = document.getElementById('json-output-scrollable');
                
                if (ocrGallery) {{
                    ocrGallery.style.maxHeight = `${{scrollableMaxHeight}}px`;
                }}
                if (jsonContainer) {{
                    jsonContainer.style.maxHeight = `${{scrollableMaxHeight}}px`;
                }}
                
                // Sync both columns to sidebar height
                resultsColumn.style.minHeight = `${{sidebarHeight}}px`;
                
                // Also set the inner container
                if (resultsContainer && resultsContainer !== resultsColumn) {{
                    resultsContainer.style.minHeight = `${{sidebarHeight}}px`;
                }}
                
                if (__observer) __observer.observe(resultsColumn, {{ childList: true, subtree: true }});
                __syncing = false;
                // #region agent log
                __agentLog('H1', 'syncColumnHeights:exit', {{
                    sidebarHeight,
                    fixedHeight,
                    scrollableMaxHeight,
                    hasOcrGallery: !!ocrGallery,
                    hasJsonContainer: !!jsonContainer,
                    sidebarFinalH: sidebar.offsetHeight,
                    resultsFinalH: resultsColumn.offsetHeight
                }});
                // #endregion
            }}

            
            if (toggleBtn && sidebar) {{
                toggleBtn.addEventListener('click', () => {{
                    sidebarVisible = !sidebarVisible;
                    const icon = toggleBtn.querySelector('.toggle-icon');
                    const text = toggleBtn.querySelector('.toggle-text');
                    
                    if (sidebarVisible) {{
                        // Show: restore display first, then animate
                        sidebar.style.display = '';
                        sidebar.classList.remove('sidebar-hidden');
                        setTimeout(() => {{
                            sidebar.style.transform = 'translateX(0)';
                            sidebar.style.opacity = '1';
                        }}, 10);
                        if (resultsColumn) {{
                            resultsColumn.style.flexGrow = '8';
                        }}
                        if (icon) icon.textContent = '◀';
                        if (text) text.textContent = 'HIDE LEFT MENU';
                        setTimeout(() => syncColumnHeights('toggle:show'), 50);
                    }} else {{
                        // Hide: animate to 90%, then apply display:none
                        sidebar.classList.add('sidebar-hidden');
                        sidebar.style.transform = 'translateX(-90%)';
                        sidebar.style.opacity = '0.3';
                        setTimeout(() => {{
                            if (!sidebarVisible) {{
                                sidebar.style.display = 'none';
                            }}
                        }}, 300);
                        if (resultsColumn) {{
                            resultsColumn.style.flexGrow = '12';
                        }}
                        if (icon) icon.textContent = '▶';
                        if (text) text.textContent = 'SHOW LEFT MENU';
                        setTimeout(() => syncColumnHeights('toggle:hide'), 350);
                    }}
                }});
            }}

            // Delay initialization to ensure Gradio components are fully rendered
            setTimeout(() => {{
                syncColumnHeights('init');
                logAlignment('init');
            }}, 100);
            requestAnimationFrame(() => logAlignment('raf'));
            window.addEventListener('resize', () => {{
                clearTimeout(window.__syncColumnsTimer);
                // #region agent log
                __agentLog('H2', 'window:resize', {{
                    width: window.innerWidth,
                    height: window.innerHeight,
                    dpr: window.devicePixelRatio
                }});
                // #endregion
                window.__syncColumnsTimer = setTimeout(() => syncColumnHeights('resize'), 100);
            }});

            if (resultsColumn) {{
                __observer = new MutationObserver((mutations) => {{
                    // #region agent log
                    __agentLog('H3', 'resultsColumn:mutations', {{
                        count: mutations.length
                    }});
                    // #endregion
                    syncColumnHeights('mutations');
                    logAlignment('mutations');
                }});
                __observer.observe(resultsColumn, {{ childList: true, subtree: true }});
            }}
            
            const tooltipTexts = {TOOLTIP_RADIO};
            let tooltip = document.getElementById("custom-tooltip");
            if (!tooltip) {{
                tooltip = document.createElement("div");
                tooltip.id = "custom-tooltip";
                tooltip.style.position = "fixed";
                tooltip.style.background = "#ffffff";
                tooltip.style.color = "#140E35";
                tooltip.style.padding = "8px 12px";
                tooltip.style.borderRadius = "6px";
                tooltip.style.fontSize = "13px";
                tooltip.style.maxWidth = "300px";
                tooltip.style.zIndex = "10000";
                tooltip.style.pointerEvents = "none";
                tooltip.style.transition = "opacity 0.2s";
                tooltip.style.opacity = "0";
                tooltip.style.whiteSpace = "normal";
                tooltip.style.boxShadow = "0 2px 8px rgba(0, 0, 0, 0.15)";
                tooltip.style.border = "1px solid #E8EDF6";
                document.body.appendChild(tooltip);
            }}
            Object.keys(tooltipTexts).forEach(id => {{
                const elem = document.getElementById(id);
                if (!elem) return;
                function showTooltip(e) {{
                    tooltip.style.opacity = "1";
                    tooltip.innerText = tooltipTexts[id];
                    let x = e.clientX + 10;
                    let y = e.clientY + 10;
                    if (x + tooltip.offsetWidth > window.innerWidth) {{
                        x = e.clientX - tooltip.offsetWidth - 10;
                    }}
                    if (y + tooltip.offsetHeight > window.innerHeight) {{
                        y = e.clientY - tooltip.offsetHeight - 10;
                    }}
                    tooltip.style.left = x + "px";
                    tooltip.style.top = y + "px";
                }}
                function hideTooltip() {{
                    tooltip.style.opacity = "0";
                }}
                elem.addEventListener("mousemove", showTooltip);
                elem.addEventListener("mouseleave", hideTooltip);
            }});
        }}
        """,
    )

if __name__ == "__main__":
    t = threading.Thread(target=delete_file_periodically, daemon=True)
    t.start()

    allowed_dirs = [
        str(BASE_DIR / "res"), 
        str(BASE_DIR / "examples"),
        str(BASE_DIR / "examples_pdf"),
    ]

    demo.launch(
        server_name="0.0.0.0",
        server_port=7860,
        show_error=True,
        inbrowser=False,
        allowed_paths=allowed_dirs,
    )