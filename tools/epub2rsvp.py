#!/usr/bin/env python3
"""
Convert a pandoc-generated markdown file (from epub) to clean RSVP text.

Usage:
    pandoc input.epub -o raw.txt --to markdown --wrap=none
    python3 epub2rsvp.py raw.txt output.txt
"""

import re
import sys


BACKMATTER = {"bibliography", "acknowledgements", "acknowledgments", "notes", "index"}


def clean_heading_text(text):
    # Remove {#id .class} attributes
    text = re.sub(r'\{[^}]*\}', '', text)
    # Remove inline HTML spans like []{#...} and bare []
    text = re.sub(r'\[\]\{[^}]*\}', '', text)
    text = re.sub(r'\[\]', '', text)
    # Remove footnote references like [31](#notes...)
    text = re.sub(r'\[\d+\]\([^)]*\)', '', text)
    # Remove markdown links, keep link text
    text = re.sub(r'\[([^\]]*)\]\([^)]*\)', r'\1', text)
    # Remove emphasis markers
    text = re.sub(r'[*_]{1,2}([^*_]+)[*_]{1,2}', r'\1', text)
    return text.strip()


def clean_paragraph(text):
    # Remove inline HTML spans (page breaks etc.)
    text = re.sub(r'\[\]\{[^}]*\}', '', text)
    # Remove footnote references
    text = re.sub(r'\[\d+\]\([^)]*\)', '', text)
    # Remove markdown links, keep link text
    text = re.sub(r'\[([^\]]*)\]\([^)]*\)', r'\1', text)
    # Remove remaining inline HTML tags
    text = re.sub(r'<[^>]+>', '', text)
    # Remove emphasis but keep text
    text = re.sub(r'[*_]{1,2}([^*_\n]+)[*_]{1,2}', r'\1', text)
    # Normalize dashes
    text = text.replace('—', ' - ').replace('–', '-')
    return text.strip()


def is_chapter_number_line(text):
    # Matches "1." or "2." etc. with nothing else meaningful
    return bool(re.match(r'^\d+\.\s*$', text))


def convert(input_path, output_path):
    with open(input_path, encoding='utf-8') as f:
        lines = f.readlines()

    out = []
    i = 0
    in_backmatter = False
    pending_chapter_num = None

    while i < len(lines):
        line = lines[i].rstrip('\n')

        # Skip fenced divs, HTML blocks, raw html spans
        if line.startswith(':::') or line.startswith('<') or line.startswith('![') or line.startswith('`'):
            i += 1
            continue

        # Skip nav/page-list blocks (large HTML lists)
        if re.match(r'^\d+\.\s+\[', line):
            i += 1
            continue

        # Heading lines
        m = re.match(r'^(#{1,4})\s+(.*)', line)
        if m:
            level = len(m.group(1))
            raw = clean_heading_text(m.group(2))

            if not raw:
                i += 1
                continue

            # Detect chapter number-only lines like "## 1."
            if is_chapter_number_line(raw):
                pending_chapter_num = raw.rstrip('.')
                i += 1
                continue

            # Check for backmatter
            if raw.lower() in BACKMATTER:
                in_backmatter = True
                i += 1
                continue

            if in_backmatter:
                i += 1
                continue

            # Combine pending chapter number with title
            if pending_chapter_num and level == 2:
                raw = f"{pending_chapter_num}. {raw}"
                pending_chapter_num = None

            # Map heading levels: ## → #, ### → ##
            new_level = max(1, level - 1)
            out.append('#' * new_level + ' ' + raw)
            out.append('')
            i += 1
            continue

        if in_backmatter:
            i += 1
            continue

        # Blank lines
        if not line.strip():
            # Avoid consecutive blank lines
            if out and out[-1] != '':
                out.append('')
            i += 1
            continue

        # Regular paragraph line
        cleaned = clean_paragraph(line)
        if cleaned:
            out.append(cleaned)

        i += 1

    # Strip leading blank lines
    while out and out[0] == '':
        out.pop(0)
    # Strip trailing blank lines
    while out and out[-1] == '':
        out.pop()

    with open(output_path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(out) + '\n')

    print(f"Done: {output_path}")
    chapters = sum(1 for l in out if l.startswith('# '))
    sections = sum(1 for l in out if l.startswith('## '))
    print(f"  {chapters} chapters, {sections} sections, {len(out)} lines")


if __name__ == '__main__':
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} input.txt output.txt")
        sys.exit(1)
    convert(sys.argv[1], sys.argv[2])
