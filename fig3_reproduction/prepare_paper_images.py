#!/usr/bin/env python3
"""Render the two source artifacts used by the Fig. 3 comparison."""

from pathlib import Path
import pymupdf

HERE = Path(__file__).resolve().parent
PDF = HERE.parent / "pdf" / "1-s2.0-S0168900211018134-main.pdf"
OUT = HERE / "paper"
OUT.mkdir(parents=True, exist_ok=True)

doc = pymupdf.open(PDF)
matrix = pymupdf.Matrix(4, 4)

# Coordinates are in PDF points and deliberately include captions/equation
# numbers so the extracted artifacts remain self-identifying.
artifacts = [
    (2, pymupdf.Rect(285, 500, 585, 610), "equations_10_11.png"),
    (3, pymupdf.Rect(25, 25, 340, 535), "paper_fig3.png"),
]
for page_index, clip, name in artifacts:
    page = doc[page_index]
    page.get_pixmap(matrix=matrix, clip=clip, alpha=False).save(OUT / name)
    print(OUT / name)
