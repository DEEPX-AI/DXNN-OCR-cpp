import unicodedata
from typing import Optional

try:
    import jiwer
except ImportError:
    jiwer = None  # type: ignore


def normalize_text_research_standard(text: str) -> str:
    """
    归一化：
    - NFKC Unicode 归一化
    - 转小写
    - 去除标点、符号及所有空白
    """
    if not isinstance(text, str):
        return ""

    text = unicodedata.normalize("NFKC", text)
    text = text.lower()

    punctuation_to_remove = (
        "＂＃＄％＆＇（）＊＋，－．／：；＜＝＞？＠［＼］＾＿｀｛｜｝～"
        "·｜「」『』《》〈〉（）"
        ".,;:!?\"'()[]{}<>@#$%^&*-_=+|\\`~"
        "●"
    )
    whitespace_to_remove = " \t\n\r\f\v"
    translator = str.maketrans("", "", punctuation_to_remove + whitespace_to_remove)
    return text.translate(translator)


def character_accuracy_from_cer(reference: str, hypothesis: str) -> Optional[float]:
    """
    使用 jiwer 计算 CER，再得到 character accuracy = 1 - CER。
    返回 0~100 的百分比，无法计算时返回 None。
    """
    if jiwer is None:
        return None

    ref_norm = normalize_text_research_standard(reference)
    hyp_norm = normalize_text_research_standard(hypothesis)

    if len(ref_norm) == 0:
        return 100.0 if len(hyp_norm) == 0 else 0.0

    error = jiwer.cer(ref_norm, hyp_norm)
    accuracy = max(0.0, 1.0 - error)
    return round(accuracy * 100.0, 2)
