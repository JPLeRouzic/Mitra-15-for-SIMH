#!/usr/bin/env python3
"""
split_comments.py

Reads a text file line by line. For any line A that contains a semicolon
NOT at the very start of the line (i.e. a trailing/inline comment), the
line is split into two new lines:

    B = the semicolon and everything after it (the comment), placed first
    C = everything in line A before the semicolon (trimmed), placed second

Line A itself is discarded and replaced by B followed by C.

Lines that have no semicolon, or whose semicolon is already the first
non-whitespace character (i.e. the whole line is already just a comment),
are copied through unchanged.

Usage:
    python3 split_comments.py input.txt [output.txt]

If output.txt is omitted, the result is printed to stdout.
"""

import sys


def split_line(line):
    """
    Given a single line (without trailing newline), return a list of
    one or two lines representing the result of the transformation.
    """
    # Find the first semicolon
    idx = line.find(';')

    if idx == -1:
        # No semicolon at all: line unchanged
        return [line]

    # Check whether the semicolon is the first non-whitespace character.
    # If so, the whole line is already just a comment - leave it alone.
    stripped_before = line[:idx]
    if stripped_before.strip() == '':
        return [line]

    # We have a semicolon "in the middle" of the line.
    comment_part = line[idx:]          # B: starts with ';'
    code_part = line[:idx].rstrip()    # C: everything before ';', trimmed

    return [comment_part, code_part]


def process_file(input_path, output_path=None):
    with open(input_path, 'r', encoding='utf-8') as f:
        lines = f.read().splitlines()

    result_lines = []
    for line in lines:
        result_lines.extend(split_line(line))

    output_text = '\n'.join(result_lines) + '\n'

    if output_path:
        with open(output_path, 'w', encoding='utf-8') as f:
            f.write(output_text)
    else:
        sys.stdout.write(output_text)


def main():
    if len(sys.argv) < 2:
        print("Usage: python3 split_comments.py input.txt [output.txt]",
              file=sys.stderr)
        sys.exit(1)

    input_path = sys.argv[1]
    output_path = sys.argv[2] if len(sys.argv) > 2 else None
    process_file(input_path, output_path)


if __name__ == '__main__':
    main()
