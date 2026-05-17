"""
ビットマップフォントアトラス生成スクリプト

256x96 ピクセル (16列 x 6行) のモノスペースフォントアトラスを生成する。
ASCII 32 (space) から 126 (~) まで 95 文字 + 最終セル (全白, ソリッド矩形用)。
白グリフ on 透明背景で出力し、頂点カラーで着色可能にする。
"""

from PIL import Image, ImageDraw, ImageFont
import os
import sys

ATLAS_WIDTH   = 256
ATLAS_HEIGHT  = 96
GLYPH_WIDTH   = 16
GLYPH_HEIGHT  = 16
COLUMNS       = 16
ROWS          = 6
FIRST_CHAR    = 32   # space
LAST_CHAR     = 126  # ~

def find_monospace_font():
    """利用可能なモノスペースフォントを探す"""
    candidates = [
        "/System/Library/Fonts/Menlo.ttc",
        "/System/Library/Fonts/Monaco.dfont",
        "/System/Library/Fonts/Courier.dfont",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
        "C:/Windows/Fonts/consola.ttf",
        "C:/Windows/Fonts/cour.ttf",
    ]
    for path in candidates:
        if os.path.exists(path):
            return path
    return None

def generate_font_atlas(output_path):
    # RGBA で透明背景
    img = Image.new("RGBA", (ATLAS_WIDTH, ATLAS_HEIGHT), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    # モノスペースフォントを探してロード (見つからなければデフォルト)
    font_path = find_monospace_font()
    font_size = 12
    if font_path:
        try:
            font = ImageFont.truetype(font_path, font_size)
        except Exception:
            font = ImageFont.load_default()
    else:
        font = ImageFont.load_default()

    # ASCII 32-126 の 95 文字をグリッドに配置
    for i in range(LAST_CHAR - FIRST_CHAR + 1):
        ch = chr(FIRST_CHAR + i)
        col = i % COLUMNS
        row = i // COLUMNS

        cell_x = col * GLYPH_WIDTH
        cell_y = row * GLYPH_HEIGHT

        # グリフのバウンディングボックスを取得してセル中央に配置
        bbox = draw.textbbox((0, 0), ch, font=font)
        glyph_w = bbox[2] - bbox[0]
        glyph_h = bbox[3] - bbox[1]

        offset_x = cell_x + (GLYPH_WIDTH - glyph_w) // 2 - bbox[0]
        offset_y = cell_y + (GLYPH_HEIGHT - glyph_h) // 2 - bbox[1]

        # 白色で描画 (alpha がグリフマスクになる)
        draw.text((offset_x, offset_y), ch, fill=(255, 255, 255, 255), font=font)

    # 最終セル (index 95): 全白ピクセル → ソリッドカラー矩形の UV 源
    solid_col = 95 % COLUMNS  # = 15
    solid_row = 95 // COLUMNS  # = 5
    solid_x = solid_col * GLYPH_WIDTH
    solid_y = solid_row * GLYPH_HEIGHT
    draw.rectangle(
        [solid_x, solid_y, solid_x + GLYPH_WIDTH - 1, solid_y + GLYPH_HEIGHT - 1],
        fill=(255, 255, 255, 255)
    )

    img.save(output_path, "PNG")
    print(f"Font atlas generated: {output_path} ({ATLAS_WIDTH}x{ATLAS_HEIGHT})")

if __name__ == "__main__":
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(script_dir)
    output = os.path.join(project_root, "Assets", "Fonts", "MonoFont.png")

    if len(sys.argv) > 1:
        output = sys.argv[1]

    generate_font_atlas(output)
