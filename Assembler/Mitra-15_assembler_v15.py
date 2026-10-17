#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
MITRA-15 Assembler
Two-pass assembler with support for forward references.
"""
import sys
import re
import struct

def pack_mitra_word(value):
    """Packs a 16-bit value into bytes (Big-Endian)."""
    if not (0 <= value <= 0xFFFF):
        raise ValueError(f"Value {value} out of range.")
    return struct.pack('>H', value)

class MITRA15Assembler:
    def __init__(self, source_file):
        self.source_file = source_file
        self.lines = []
        # Symbol tables
        self.labels = {}          # label -> value
        self.external_refs = set()  # labels referenced as external
        self.lds_names = set()    # Names of LDS sections
        # Assembler state
        self.output = bytearray()
        self.current_address = 0
        self.in_lds = False
        self.in_lps = False
        self.current_lds = None
        self.section_offset = 0  # Offset for current section
        self.current_section = None  # 'CDS', 'LDS', or 'LPS'
        # Register names - these are not labels
        self.registers = {
            'G_REG', 'L_REG', 'X_REG', 'E_REG', 'A_REG',
            'G', 'L', 'X', 'E', 'A'
        }
        # Operation labels (M:xx)
        self.op_labels = {
            'M:BI': 1, 'M:BO': 2, 'M:CI': 3, 'M:OC': 4,
            'M:EI': 5, 'M:EO': 6, 'M:LO': 7, 'M:LL': 8,
            'M:DO': 9, 'M:SI': 10, 'M:SL': 11, 'M:UL': 12,
            'M:SY': 13, 'M:EP': 14, 'M:GI': 15, 'M:GO': 16,
            'M:IO': 0, 'M:WAIT': 0, 'M:ZIO': 0,
            'M:ZWAT': 0, 'M:IO2': 0
        }
        for i in range(17, 32):
            self.op_labels[f'U:F{i-16:X}'] = i
        # Add user labels U:1..U:47
        for i in range(1, 48):
            self.op_labels[f'U:{i}'] = i
            
        self.known_opcodes = {
            # Load/Store
            'LBL', 'SBL', 'LBR', 'SBR', 'LBX', 'LDA', 'STA',
            'LDE', 'STE', 'LDX', 'STX', 'LDR', 'STR', 'LEA',
            'SPA', 'STS', 'DLD', 'DST',
            # Arithmetic
            'ADD', 'ADM', 'SUB', 'MUL', 'DIV',
            # Logical
            'IOR', 'EOR', 'AND', 'CMP',
            # Increment/Decrement
            'ICX', 'DCX', 'ICL', 'DCL',
            # Shifts
            'SHR', 'SHC', 'SRG',
            # Shifts - derived mnemonics
            'SLLS', 'SRCS', 'SAD', 'SLCD', 'SLCS', 'SAS', 'SRLS', 'SRCD',
            'SLLD', 'SRLD', 'PTY', 'NLZ',
            # Register operations
            'XAE', 'XAX', 'XEX', 'XAA', 'CCE', 'ACE', 'CCA', 'AEE',
            'CNX', 'AIE', 'AAE', 'LNE', 'CNA', 'CHX',
            # Floating point
            'FAD', 'FSU', 'FMU', 'FDV',
            # String
            'MVS', 'CPS', 'TRS',
            # Branches
            'BRU', 'BRX', 'BCT', 'BOT', 'BCF', 'BOF',
            'BAZ', 'BAN', 'BE', 'BZ', 'BL', 'BLZ',
            'BNE', 'BNZ', 'BGE', 'BPZ',
            # System
            'CLS', 'RTS', 'CSV', 'RSV', 'DIT', 'DITR',
            # Control
            'TES', 'STM', 'CLM', 'RD', 'WD', 'LDP',
            # Directives
            'DATA', 'TEXT', 'RES', 'BND', 'EQU', 'ORG',
            'CDS', 'LDS', 'IDS', 'LPS', 'FIN', 'END',
            'DEF', 'REF', 'PAGE', 'GOTO', 'DO', 'BASE'
        }
        self.opcode_hex = {
            'LBL': {'DL': 0x0D, 'P': 0x2D, 'DG': 0x4D, 'IL': 0x6D, 'IGX': 0x8D, 'ILX': 0xAD},
            'SBL': {'DL': 0x14, 'DG': 0x54, 'IL': 0x74, 'IGX': 0x94, 'ILX': 0xB4},
            'LBR': {'DL': 0x0E, 'P': 0x2E, 'DG': 0x4E, 'IL': 0x6E, 'IGX': 0x8E, 'ILX': 0xAE},
            'SBR': {'DL': 0x15, 'DG': 0x55, 'IL': 0x75, 'IGX': 0x95, 'ILX': 0xB5},
            'LBX': {'DL': 0x0F, 'P': 0x2F, 'DG': 0x4F, 'IL': 0x6F, 'IGX': 0x8F, 'ILX': 0xAF},
            'LDA': {'DL': 0x00, 'P': 0x20, 'DG': 0x40, 'IL': 0x60, 'IGX': 0x80, 'ILX': 0xA0},
            'STA': {'DL': 0x11, 'DG': 0x51, 'IL': 0x71, 'IGX': 0x91, 'ILX': 0xB1},
            'LDE': {'DL': 0x01, 'P': 0x21, 'DG': 0x41, 'IL': 0x61, 'IGX': 0x81, 'ILX': 0xA1},
            'STE': {'DL': 0x12, 'DG': 0x52, 'IL': 0x72, 'IGX': 0x92, 'ILX': 0xB2},
            'LDX': {'DL': 0x02, 'P': 0x22, 'DG': 0x42, 'IL': 0x62, 'IGX': 0x82, 'ILX': 0xA2},
            'STX': {'DL': 0x13, 'DG': 0x53, 'IL': 0x73, 'IGX': 0x93, 'ILX': 0xB3},
            'LDR': {'DL': 0x39, 'PX': 0xE9, 'P': 0xF9},
            'STR': {'DL': 0x3A, 'PX': 0xEA, 'P': 0xFA},
            'LEA': {'DL': 0x04, 'P': 0x24, 'DG': 0x44, 'IL': 0x64, 'IGX': 0x84, 'ILX': 0xA4},
            'SPA': {'DL': 0x18, 'DG': 0x58, 'IL': 0x78, 'IGX': 0x98, 'ILX': 0xB8},
            'STS': {'DL': 0x19, 'DG': 0x59, 'IL': 0x79, 'IGX': 0x99, 'ILX': 0xB9},
            'DLD': {'DL': 0x10, 'DG': 0x50, 'IL': 0x70, 'IGX': 0x90, 'ILX': 0xB0},
            'DST': {'DL': 0x16, 'DG': 0x56, 'IL': 0x76, 'IGX': 0x96, 'ILX': 0xB6},
            'ADD': {'DL': 0x05, 'P': 0x25, 'DG': 0x45, 'IL': 0x65, 'IGX': 0x85, 'ILX': 0xA5},
            'ADM': {'DL': 0x17, 'DG': 0x57, 'IL': 0x77, 'IGX': 0x97, 'ILX': 0xB7},
            'SUB': {'DL': 0x06, 'P': 0x26, 'DG': 0x46, 'IL': 0x66, 'IGX': 0x86, 'ILX': 0xA6},
            'MUL': {'DL': 0x0C, 'P': 0x2C, 'DG': 0x4C, 'IL': 0x6C, 'IGX': 0x8C, 'ILX': 0xAC},
            'DIV': {'DL': 0x08, 'P': 0x28, 'DG': 0x48, 'IL': 0x68, 'IGX': 0x88, 'ILX': 0xA8},
            'IOR': {'DL': 0x07, 'P': 0x27, 'DG': 0x47, 'IL': 0x67, 'IGX': 0x87, 'ILX': 0xA7},
            'EOR': {'DL': 0x03, 'P': 0x23, 'DG': 0x43, 'IL': 0x63, 'IGX': 0x83, 'ILX': 0xA3},
            'AND': {'DL': 0x09, 'P': 0x29, 'DG': 0x49, 'IL': 0x69, 'IGX': 0x89, 'ILX': 0xA9},
            'CMP': {'DL': 0x0B, 'P': 0x2B, 'DG': 0x4B, 'IL': 0x6B, 'IGX': 0x8B, 'ILX': 0xAB},
            'ICX': {'DL': 0x32, 'PX': 0xE2, 'P': 0xF2},
            'DCX': {'DL': 0x33, 'PX': 0xE3, 'P': 0xF3},
            'ICL': {'DL': 0x35, 'PX': 0xE5, 'P': 0xF5},
            'DCL': {'DL': 0x36, 'PX': 0xE6, 'P': 0xF6},
            'SHR': {'DL': 0x30, 'PX': 0xE0, 'P': 0xF0},
            'SHC': {'DL': 0x3C, 'PX': 0xEC, 'P': 0xFC},
            'SRG': {'DL': 0x31, 'PX': 0xE1, 'P': 0xF1},
            'FAD': {'DL': 0x1A, 'DG': 0x5A, 'IL': 0x7A, 'IGX': 0x9A, 'ILX': 0xBA},
            'FSU': {'DL': 0x1B, 'DG': 0x5B, 'IL': 0x7B, 'IGX': 0x9B, 'ILX': 0xBB},
            'FMU': {'DL': 0x1C, 'DG': 0x5C, 'IL': 0x7C, 'IGX': 0x9C, 'ILX': 0xBC},
            'FDV': {'DL': 0x1D, 'DG': 0x5D, 'IL': 0x7D, 'IGX': 0x9D, 'ILX': 0xBD},
            'MVS': {'DL': 0x1F, 'DG': 0x5F, 'IL': 0x7F, 'IGX': 0x9F, 'ILX': 0xBF},
            'CPS': {'DL': 0x0A, 'P': 0x2A, 'DG': 0x4A, 'IL': 0x6A, 'IGX': 0x8A, 'ILX': 0xAA},
            'TRS': {'DL': 0x1E, 'DG': 0x5E, 'IL': 0x7E, 'IGX': 0x9E, 'ILX': 0xBE},
            'BRU': {'RP': 0xC7, 'RM': 0xCF, 'IL': 0xD7, 'IG': 0xDF},
            'BRX': {'RP': 0xC1, 'RM': 0xC9, 'IL': 0xD1, 'IG': 0xD9},
            'BCT': {'RP': 0xC0, 'RM': 0xC8, 'IL': 0xD0, 'IG': 0xD8},
            'BOT': {'RP': 0xC2, 'RM': 0xCA, 'IL': 0xD2, 'IG': 0xDA},
            'BCF': {'RP': 0xC3, 'RM': 0xCB, 'IL': 0xD3, 'IG': 0xDB},
            'BOF': {'RP': 0xC6, 'RM': 0xCE, 'IL': 0xD6, 'IG': 0xDE},
            'BAZ': {'RP': 0xC5, 'RM': 0xCD, 'IL': 0xD5, 'IG': 0xDD},
            'BAN': {'RP': 0xC4, 'RM': 0xCC, 'IL': 0xD4, 'IG': 0xDC},
            'BE':  {'RP': 0xC0, 'RM': 0xC8, 'IL': 0xD0, 'IG': 0xD8},
            'BZ':  {'RP': 0xC0, 'RM': 0xC8, 'IL': 0xD0, 'IG': 0xD8},
            'BL':  {'RP': 0xC2, 'RM': 0xCA, 'IL': 0xD2, 'IG': 0xDA},
            'BLZ': {'RP': 0xC2, 'RM': 0xCA, 'IL': 0xD2, 'IG': 0xDA},
            'BNE': {'RP': 0xC3, 'RM': 0xCB, 'IL': 0xD3, 'IG': 0xDB},
            'BNZ': {'RP': 0xC3, 'RM': 0xCB, 'IL': 0xD3, 'IG': 0xDB},
            'BGE': {'RP': 0xC6, 'RM': 0xCE, 'IL': 0xD6, 'IG': 0xDE},
            'BPZ': {'RP': 0xC6, 'RM': 0xCE, 'IL': 0xD6, 'IG': 0xDE},
            'CLS': {'DL': 0x38, 'PX': 0xE8, 'P': 0xF8},
            'RTS': {'P': 0xF100},
            'CSV': {'DL': 0x37, 'PX': 0xE7, 'P': 0xF7},
            'RSV': {'P': 0xF10C},
            'DIT': {'P': 0xF401},
            'DITR': {'DL': 0x3C20, 'PX': 0xEC20, 'P': 0xFC20},
            'TES': {'DL': 0x3D, 'PX': 0xED, 'P': 0xFD},
            'STM': {'P': 0xF408},
            'CLM': {'P': 0xF400},
            'RD':  {'P': 0xF402},
            'WD':  {'P': 0xF403},
            'LDP': {'DL': 0x3B, 'PX': 0xEB, 'P': 0xFB},
        }
        # Derived opcodes for shifts
        self.derived_shifts = {
            'SLLS': 0x00, 'SRCS': 0x01, 'SAD': 0x02, 'SLCD': 0x03,
            'SLCS': 0x04, 'SAS': 0x05, 'SRLS': 0x06, 'SRCD': 0x07,
        }
        self.derived_shc = {
            'SLLD': 0x00, 'PTY': 0x02, 'SRLD': 0x04, 'NLZ': 0x06,
        }
        self.derived_srg = {
            'XAE': 0x01, 'XAX': 0x02, 'XEX': 0x03, 'XAA': 0x04,
            'CCE': 0x05, 'RSV': 0x06, 'ACE': 0x07, 'CCA': 0x08,
            'AEE': 0x09, 'CNX': 0x0A, 'AIE': 0x0B, 'AAE': 0x0C,
            'LNE': 0x0D, 'CNA': 0x0E, 'CHX': 0x0F,
        }
        # Directives that don't generate code
        self.directives = {
            'EQU', 'ORG', 'CDS', 'LDS', 'IDS', 'LPS', 'FIN', 'END',
            'DEF', 'REF', 'PAGE', 'GOTO', 'DO', 'BASE', 'BND'
        }

    def read_source(self):
        """Read source file lines, handling comments and preprocessing listing formats."""
        try:
            with open(self.source_file, 'r', encoding='utf-8') as f:
                raw_lines = []
                for line_num, line in enumerate(f, 1):
                    # Handle comments - careful with quoted strings
                    comment_index = -1
                    in_quotes = False
                    for i, ch in enumerate(line):
                        if ch == '"':
                            in_quotes = not in_quotes
                        elif ch == ';' and not in_quotes:
                            comment_index = i
                            break
                        elif ch == '*' and not in_quotes and not line[:i].strip():
                            # '*' is a comment marker only in column 1; elsewhere it is
                            # the indirect-pointer prefix (e.g. LDA IL *V_0002)
                            comment_index = i
                            break
                    if comment_index != -1:
                        line = line[:comment_index]
                    line = line.strip()
                    if line:
                        raw_lines.append((line_num, line))
            
            # Preprocess lines to handle address labels, hex addresses, hex dumps, and .WORD/.BYTE/.ZERO
            self.lines = []
            MODES = {'DL', 'DG', 'IL', 'IG', 'P', 'PX', 'ILX', 'IGX', 'RP', 'RM'}
            for line_num, line in raw_lines:
                tokens = line.split()
                if not tokens:
                    self.lines.append((line_num, line))
                    continue
                
                first_tok = tokens[0]
                if first_tok.endswith(':'):
                    prefix = first_tok[:-1]
                    # Check if it's a hex address (e.g., 000006:)
                    if prefix and all(c in '0123456789ABCDEFabcdef' for c in prefix):
                        # It's a hex address
                        if len(tokens) == 1:
                            self.lines.append((line_num, f"ORG &{prefix}"))
                            continue
                        
                        second_tok = tokens[1]
                        is_hex_dump = False
                        # Check if the second token is a hex dump (pure hex) and followed by an opcode
                        if all(c in '0123456789ABCDEFabcdef' for c in second_tok):
                            if len(tokens) > 2:
                                third_tok = tokens[2].upper()
                                if third_tok in self.known_opcodes or third_tok.startswith('.') or third_tok in ('DATA', 'TEXT', 'RES'):
                                    is_hex_dump = True
                        
                        if is_hex_dump:
                            rest_tokens = tokens[2:]
                        else:
                            rest_tokens = tokens[1:]
                            
                        # Handle explicit mode tokens from disassembler output
                        if len(rest_tokens) >= 3 and rest_tokens[1].upper() in MODES:
                            mode = rest_tokens[1].upper()
                            val = rest_tokens[2]
                            if mode == 'DG':
                                if not val.startswith('#'): val = '#' + val
                            elif mode == 'IL':
                                if not val.startswith('@'): val = '@' + val
                            elif mode == 'IG':
                                if not val.startswith('@#'):
                                    if val.startswith('#'): val = '@' + val
                                    else: val = '@#' + val
                            elif mode == 'P':
                                if not val.startswith('='): val = '=' + val
                            elif mode in ('PX', 'ILX', 'IGX'):
                                if mode == 'PX' and not val.startswith('='): val = '=' + val
                                elif mode == 'ILX' and not val.startswith('@'): val = '@' + val
                                elif mode == 'IGX':
                                    if not val.startswith('@#'):
                                        if val.startswith('#'): val = '@' + val
                                        else: val = '@#' + val
                                if not val.upper().endswith(',X'): val += ',X'
                            
                            rest_tokens = [rest_tokens[0], val] + rest_tokens[3:]

                        new_tokens = []
                        for t in rest_tokens:
                            t_upper = t.upper()
                            if t_upper == '.WORD':
                                new_tokens.append('DATA')
                            elif t_upper == '.BYTE':
                                new_tokens.append('DATA,1')
                            elif t_upper == '.ZERO':
                                new_tokens.append('RES')
                            else:
                                new_tokens.append(t)
                        rest_of_line = ' '.join(new_tokens)
                        
                        self.lines.append((line_num, f"ORG &{prefix}"))
                        if rest_of_line:
                            self.lines.append((line_num, rest_of_line))
                        continue
                    else:
                        # It's a standard label ending with ':' (e.g., V_0006:)
                        new_tokens = [prefix] + tokens[1:]
                        final_tokens = []
                        for t in new_tokens:
                            t_upper = t.upper()
                            if t_upper == '.WORD':
                                final_tokens.append('DATA')
                            elif t_upper == '.BYTE':
                                final_tokens.append('DATA,1')
                            elif t_upper == '.ZERO':
                                final_tokens.append('RES')
                            else:
                                final_tokens.append(t)
                        line = ' '.join(final_tokens)
                        self.lines.append((line_num, line))
                        continue
                
                # If it doesn't end with ':', just replace .WORD/.BYTE/.ZERO if present
                new_tokens = []
                for t in tokens:
                    t_upper = t.upper()
                    if t_upper == '.WORD':
                        new_tokens.append('DATA')
                    elif t_upper == '.BYTE':
                        new_tokens.append('DATA,1')
                    elif t_upper == '.ZERO':
                        new_tokens.append('RES')
                    else:
                        new_tokens.append(t)
                line = ' '.join(new_tokens)
                self.lines.append((line_num, line))
                
        except FileNotFoundError:
            print(f"Error: File '{self.source_file}' not found.")
            sys.exit(1)

    # Valid MITRA-15 identifier: letters, digits, underscore; must start with a letter.
    IDENT_RE = re.compile(r'^[A-Za-z][A-Za-z0-9_]*$')

    def parse_line(self, line):
        """Parse a source line into label, opcode, and operand."""
        if not line or line.startswith(';') or line.startswith('*'):
            return None, None, None
        tokens = line.split()
        if not tokens:
            return None, None, None
        
        label = None
        first_token = tokens[0].upper()
        
        # Check if first token is a label
        is_label = False
        if self.IDENT_RE.match(first_token):
            if first_token not in self.known_opcodes:
                is_label = True
        elif first_token.startswith('M:') or first_token.startswith('U:'):
            is_label = False
            
        if is_label:
            label = first_token
            tokens = tokens[1:]
            
        if not tokens:
            return label, None, None
            
        opcode = tokens[0].upper()
        
        if opcode in self.directives and len(tokens) == 1:
            return label, opcode, ""
            
        operand_str = ' '.join(tokens[1:])
        
        if opcode in self.known_opcodes:
            return label, opcode, operand_str
            
        if opcode in self.registers:
            if label:
                pass
            return label, None, None
            
        return label, opcode, operand_str

    # turn operand/expression text into a number
    def resolve_expression(self, expr_str, current_addr=None):
        """Resolve an expression to a numeric value."""
        if not expr_str:
            return None
        expr_str = expr_str.strip()
        
        # Strip leading '#' if present (e.g. from disassembler's P =#C8 syntax)
        if expr_str.startswith('#'):
            expr_str = expr_str[1:].strip()
        
        # Hex constant
        if expr_str.startswith('&'):
            try:
                return int(expr_str[1:], 16)
            except ValueError:
                return None
                
        # Binary constant
        if expr_str.startswith('%'):
            try:
                return int(expr_str[1:], 2)
            except ValueError:
                return None
                
        # Unprefixed digit string → treat as hexadecimal
        if expr_str.isdigit() or (len(expr_str) > 1 and all(c in '0123456789ABCDEFabcdef' for c in expr_str)):
            try:
                return int(expr_str, 16)
            except ValueError:
                return None
                
        # Decimal constant
        if expr_str.isdigit():
            try:
                return int(expr_str)
            except ValueError:
                return None
                
        # Operation label (M:xx or U:xx)
        if expr_str in self.op_labels:
            return self.op_labels[expr_str]
            
        # Character literal
        if expr_str.startswith("'") and expr_str.endswith("'") and len(expr_str) == 3:
            return ord(expr_str[1])
            
        # Handle expressions with + and -
        if '+' in expr_str or '-' in expr_str:
            eval_str = expr_str
            for label, addr in self.labels.items():
                eval_str = re.sub(r'\b' + re.escape(label) + r'\b', str(addr), eval_str)
            if current_addr is not None:
                eval_str = eval_str.replace('$', str(current_addr))
            try:
                result = eval(eval_str, {"__builtins__": {}}, {})
                if isinstance(result, int):
                    return result
            except:
                pass
                
        # Check if it's a label reference (starts with letter)
        if expr_str and expr_str[0].isalpha():
            upper_expr = expr_str.upper()
            if upper_expr in self.labels:
                return self.labels[upper_expr]
            return None
            
        return None

    def parse_operand(self, operand_str, current_addr=None):
        """Parse operand string into mode and value."""
        if not operand_str:
            return None
        operand_str = operand_str.strip()
        
        # Check for index
        is_indexed = False
        if operand_str.upper().endswith(',X'):
            is_indexed = True
            operand_str = operand_str[:-2].strip()
            
        # Check for indirect
        is_indirect = operand_str.startswith('@')
        if is_indirect:
            operand_str = operand_str[1:].strip()
            
        # Check for general mode (#) - CDS reference
        is_general = operand_str.startswith('#')
        if is_general:
            operand_str = operand_str[1:].strip()
            
        # Check for parameter (=)
        is_param = operand_str.startswith('=')
        if is_param:
            operand_str = operand_str[1:].strip()
            
        if is_param:
            mode = 'PX' if is_indexed else 'P'
            val = self.resolve_expression(operand_str, current_addr)
            if val is not None:
                return (mode, 0, val, False)
            return (mode, 0, operand_str, True)
            
        if is_indirect:
            if is_general:
                mode = 'IGX' if is_indexed else 'IG'
            else:
                mode = 'ILX' if is_indexed else 'IL'
            return (mode, 0, operand_str, True)
            
        if is_general:
            val = self.resolve_expression(operand_str, current_addr)
            if val is not None:
                return ('DG', 0, val, False)
            if operand_str in self.op_labels:
                return ('DG', 0, self.op_labels[operand_str], False)
            return ('DG', 0, operand_str, True)
            
        val = self.resolve_expression(operand_str, current_addr)
        if val is not None:
            return ('DL', 0, val, False)
        if operand_str in self.op_labels:
            return ('DL', 0, self.op_labels[operand_str], False)
        return ('DL', 0, operand_str, True)

    def parse_data_args(self, operand_str):
        """Parse arguments for DATA directive."""
        if not operand_str:
            return []
        args = []
        current = ""
        in_quotes = False
        for char in operand_str:
            if char == '"':
                in_quotes = not in_quotes
                current += char
            elif char == ',' and not in_quotes:
                if current.strip():
                    args.append(current.strip())
                current = ""
            else:
                current += char
        if current.strip():
            args.append(current.strip())
        return args

    def assemble_instruction(self, line_num, opcode, operand_str):
        """Assemble a single instruction."""
        if not opcode:
            return None
        opcode_upper = opcode.upper()
        
        # Handle directives first
        if opcode_upper == 'DATA':
            return self.assemble_data(line_num, False, operand_str)
        if opcode_upper == 'DATA,1':
            return self.assemble_data(line_num, True, operand_str)
        if opcode_upper == 'TEXT':
            return self.assemble_text(line_num, operand_str)
        if opcode_upper == 'RES':
            return self.assemble_res(line_num, False, operand_str)
        if opcode_upper == 'RES,1':
            return self.assemble_res(line_num, True, operand_str)
        if opcode_upper == 'BND':
            if self.current_address % 2 == 1:
                self.current_address += 1
            return b'\x00'
        if opcode_upper in self.directives:
            return b''
            
        # Handle derived shift mnemonics
        if opcode_upper in self.derived_shifts:
            shift_code = self.derived_shifts[opcode_upper]
            parsed = self.parse_operand(operand_str, self.current_address)
            if not parsed:
                return None
            mode, base, value, is_label = parsed
            if mode in ('PX', 'P', 'DL'):
                if is_label:
                    val = self.resolve_expression(value, self.current_address)
                    if val is None:
                        print(f"Warning: Line {line_num}: Could not resolve shift count '{value}'")
                        return None
                    value = val
                low_byte = (shift_code << 5) | (value & 0x1F)
                high_byte = {'PX': 0xE0, 'P': 0xF0, 'DL': 0x30}[mode]
                return pack_mitra_word((high_byte << 8) | low_byte)
            else:
                print(f"Warning: Line {line_num}: Unsupported addressing mode '{mode}' for {opcode_upper}")
                return None
                
        # Handle derived SHC mnemonics
        if opcode_upper in self.derived_shc:
            shc_code = self.derived_shc[opcode_upper]
            parsed = self.parse_operand(operand_str, self.current_address)
            if not parsed:
                return None
            mode, base, value, is_label = parsed
            if mode in ('PX', 'P', 'DL'):
                if is_label:
                    val = self.resolve_expression(value, self.current_address)
                    if val is None:
                        print(f"Warning: Line {line_num}: Could not resolve shift count '{value}'")
                        return None
                    value = val
                low_byte = (shc_code << 5) | (value & 0x1F)
                high_byte = {'PX': 0xEC, 'P': 0xFC, 'DL': 0x3C}[mode]
                return pack_mitra_word((high_byte << 8) | low_byte)
            else:
                print(f"Warning: Line {line_num}: Unsupported addressing mode '{mode}' for {opcode_upper}")
                return None
                
        # Handle derived SRG mnemonics
        if opcode_upper in self.derived_srg:
            srg_code = self.derived_srg[opcode_upper]
            low_byte = srg_code << 1
            high_byte = 0xF1
            if operand_str:
                first_tok = operand_str.strip().split(None, 1)[0].upper()
                high_byte = {'DL': 0x31, 'PX': 0xE1, 'P': 0xF1}.get(first_tok, 0xF1)
            if opcode_upper == 'RSV':
                low_byte = 0x0C
            if opcode_upper == 'RTS':
                low_byte = 0x00
            return pack_mitra_word((high_byte << 8) | low_byte)
            
        # Parse operand for normal instruction
        parsed = self.parse_operand(operand_str, self.current_address)
        if not parsed:
            if opcode_upper in ('RTS', 'ICX', 'DCX', 'ICL', 'DCL'):
                if opcode_upper == 'RTS':
                    return pack_mitra_word(0xF100)
                if opcode_upper == 'ICX':
                    return pack_mitra_word(0xF200)
                if opcode_upper == 'DCX':
                    return pack_mitra_word(0xF300)
                if opcode_upper == 'ICL':
                    return pack_mitra_word(0xF500)
                if opcode_upper == 'DCL':
                    return pack_mitra_word(0xF600)
            if opcode_upper in self.opcode_hex:
                entry = self.opcode_hex[opcode_upper]
                fixed_val = entry.get('P')
                if isinstance(fixed_val, int) and fixed_val > 0xFF and len(entry) == 1:
                    return pack_mitra_word(fixed_val)
            print(f"Warning: Line {line_num}: Could not parse operand '{operand_str}' for {opcode_upper}")
            return None
            
        mode, base, value, is_label = parsed
        
        # For LEA instruction - load effective address
        if opcode_upper == 'LEA':
            if is_label:
                target = self.labels.get(value.upper())
                if target is None:
                    if value in self.op_labels:
                        target = self.op_labels[value]
                    else:
                        print(f"Error: Line {line_num}: Undefined label '{value}'")
                        return None
                value = target
                is_label = False
                
        # Handle branch instructions
        BRANCH_OPCODES = {
            'BRU', 'BRX', 'BCT', 'BOT', 'BCF', 'BOF',
            'BAZ', 'BAN', 'BE', 'BZ', 'BL', 'BLZ',
            'BNE', 'BNZ', 'BGE', 'BPZ'
        }
        if opcode_upper in BRANCH_OPCODES:
            if mode in ('RP', 'RM'):
                pass
            elif mode == 'DL':
                if is_label:
                    target = self.labels.get(value.upper())
                    if target is None:
                        if value in self.op_labels:
                            target = self.op_labels[value]
                        else:
                            print(f"Error: Line {line_num}: Undefined label '{value}'")
                            return None
                else:
                    target = value
                diff = target - self.current_address
                if diff >= 0:
                    mode = 'RP'
                    value = diff // 2
                else:
                    mode = 'RM'
                    value = (-diff) // 2
                if value > 255:
                    print(f"Warning: Line {line_num}: Branch target too far (offset = {diff} bytes)")
            elif mode in ('IL', 'IG'):
                pass
            else:
                print(f"Error: Line {line_num}: Invalid addressing mode for branch")
                return None
                
        # Resolve label if needed for other instructions
        if is_label and mode not in ('P', 'PX', 'RP', 'RM'):
            target = self.labels.get(value.upper())
            if target is not None:
                value = target
                is_label = False
            elif value in self.op_labels:
                value = self.op_labels[value]
                is_label = False
                
        # Check opcode
        if opcode_upper not in self.opcode_hex:
            print(f"Warning: Line {line_num}: Unknown instruction '{opcode_upper}'")
            return None
            
        entry = self.opcode_hex[opcode_upper]
        if mode not in entry:
            if mode == 'DG' and 'DL' in entry:
                mode = 'DL'
            elif mode == 'IL' and 'DL' in entry:
                mode = 'DL'
            elif mode == 'IG' and 'IL' in entry:
                mode = 'IL'
            else:
                print(f"Warning: Line {line_num}: Unsupported addressing mode '{mode}' for {opcode_upper}")
                return None
                
        op_val = entry.get(mode)
        if op_val is None:
            print(f"Warning: Line {line_num}: Unsupported addressing mode '{mode}' for {opcode_upper}")
            return None
            
        if isinstance(op_val, int) and op_val > 0xFF:
            return pack_mitra_word(op_val)
            
        # Ensure value is an integer
        if isinstance(value, str):
            resolved = self.resolve_expression(value, self.current_address)
            if resolved is not None:
                value = resolved
            else:
                print(f"Error: Line {line_num}: Cannot resolve '{value}' to a numeric value")
                return None
                
        instruction_word = (op_val << 8) | (value & 0xFF)
        return pack_mitra_word(instruction_word)

    def assemble_data(self, line_num, byte_mode, operand_str):
        """Assemble DATA directive."""
        code = b''
        args = self.parse_data_args(operand_str)
        for arg in args:
            if arg.startswith('#'):
                arg = arg[1:]
            if arg.startswith('"') and arg.endswith('"'):
                text = arg[1:-1]
                for c in text:
                    b_val = ord(c)
                    if byte_mode:
                        code += struct.pack('>B', b_val)
                        self.current_address += 1
                    else:
                        if self.current_address % 2 == 1:
                            code += b'\x00'
                            self.current_address += 1
                        code += struct.pack('>B', b_val) + b'\x00'
                        self.current_address += 2
            else:
                val = self.resolve_expression(arg, self.current_address)
                if val is None:
                    if arg.upper() in self.labels:
                        val = self.labels[arg.upper()]
                    elif arg in self.op_labels:
                        val = self.op_labels[arg]
                    else:
                        print(f"Warning: Line {line_num}: Could not resolve expression '{arg}' for DATA")
                        val = 0
                if byte_mode:
                    code += struct.pack('>B', val & 0xFF)
                    self.current_address += 1
                else:
                    if self.current_address % 2 == 1:
                        code += b'\x00'
                        self.current_address += 1
                    code += pack_mitra_word(val & 0xFFFF)
                    self.current_address += 2
        return code

    def assemble_text(self, line_num, operand_str):
        """Assemble TEXT directive."""
        code = b''
        if operand_str.startswith('"') and operand_str.endswith('"'):
            text = operand_str[1:-1]
            i = 0
            while i < len(text):
                if text[i] == '\\' and i + 1 < len(text):
                    if text[i+1] == 'r':
                        code += b'\r'
                        self.current_address += 1
                    elif text[i+1] == 'n':
                        code += b'\n'
                        self.current_address += 1
                    elif text[i+1] == 't':
                        code += b'\t'
                        self.current_address += 1
                    elif text[i+1] == '\\':
                        code += b'\\'
                        self.current_address += 1
                    elif text[i+1] == '"':
                        code += b'"'
                        self.current_address += 1
                    else:
                        code += text[i].encode('utf-8')
                        self.current_address += 1
                    i += 2
                else:
                    code += text[i].encode('utf-8')
                    self.current_address += 1
                    i += 1
        else:
            if operand_str.startswith('"'):
                end_quote = operand_str.find('"', 1)
                if end_quote != -1:
                    text = operand_str[1:end_quote]
                    i = 0
                    while i < len(text):
                        if text[i] == '\\' and i + 1 < len(text):
                            if text[i+1] == 'r':
                                code += b'\r'
                                self.current_address += 1
                            elif text[i+1] == 'n':
                                code += b'\n'
                                self.current_address += 1
                            elif text[i+1] == 't':
                                code += b'\t'
                                self.current_address += 1
                            elif text[i+1] == '\\':
                                code += b'\\'
                                self.current_address += 1
                            elif text[i+1] == '"':
                                code += b'"'
                                self.current_address += 1
                            else:
                                code += text[i].encode('utf-8')
                                self.current_address += 1
                            i += 2
                        else:
                            code += text[i].encode('utf-8')
                            self.current_address += 1
                            i += 1
                else:
                    print(f"Warning: Line {line_num}: Invalid TEXT format")
            else:
                print(f"Warning: Line {line_num}: Invalid TEXT format")
        return code

    def assemble_res(self, line_num, byte_mode, operand_str):
        """Assemble RES directive (reserve space)."""
        count = self.resolve_expression(operand_str, self.current_address)
        if count is None:
            if operand_str.upper() in self.labels:
                count = self.labels[operand_str.upper()]
            else:
                count = 1
        code = b''
        if byte_mode:
            code += b'\x00' * count
            self.current_address += count
        else:
            if self.current_address % 2 == 1:
                code += b'\x00'
                self.current_address += 1
            code += b'\x00\x00' * count
            self.current_address += count * 2
        return code

    def first_pass(self):
        """First pass: collect all labels and their addresses."""
        self.labels.clear()
        self.current_address = 0
        self.current_section = None
        
        # First, collect all EQU definitions
        for line_num, line in self.lines:
            label, opcode, operand_str = self.parse_line(line)
            if opcode == 'EQU' and label:
                val = self.resolve_expression(operand_str, self.current_address)
                if val is not None:
                    self.labels[label.upper()] = val
                    
        # Now do the full first pass
        self.current_address = 0
        for line_num, line in self.lines:
            label, opcode, operand_str = self.parse_line(line)
            
            if opcode == 'CDS':
                self.current_address = 0
                self.current_section = 'CDS'
                if label:
                    self.labels[label.upper()] = 0
                continue
            if opcode == 'LDS':
                self.current_address = 0
                self.current_section = 'LDS'
                if label:
                    self.labels[label.upper()] = 0
                continue
            if opcode == 'IDS':
                self.current_address = 0
                self.current_section = 'IDS'
                if label:
                    self.labels[label.upper()] = 0
                continue
            if opcode == 'LPS':
                self.current_address = 0
                self.current_section = 'LPS'
                if label:
                    self.labels[label.upper()] = 0
                continue
            if opcode == 'FIN':
                if label:
                    self.labels[label.upper()] = self.current_address
                self.current_address = 0
                continue
            if opcode == 'END':
                break
            if opcode == 'ORG':
                val = self.resolve_expression(operand_str, self.current_address)
                if val is not None:
                    self.current_address = val
                continue
                
            if label and opcode not in ('EQU', 'ORG', 'CDS', 'LDS', 'IDS', 'LPS', 'FIN', 'END'):
                if label.upper() not in self.labels:
                    self.labels[label.upper()] = self.current_address
                    
            if opcode in ('DATA', 'DATA,1'):
                byte_mode = (opcode == 'DATA,1')
                args = self.parse_data_args(operand_str)
                for arg in args:
                    if arg.startswith('#'):
                        arg = arg[1:]
                    if arg.startswith('"') and arg.endswith('"'):
                        length = len(arg) - 2
                        if byte_mode:
                            self.current_address += length
                        else:
                            if self.current_address % 2 == 1:
                                self.current_address += 1
                            self.current_address += length * 2
                    else:
                        if byte_mode:
                            self.current_address += 1
                        else:
                            if self.current_address % 2 == 1:
                                self.current_address += 1
                            self.current_address += 2
                continue
                
            if opcode in ('RES', 'RES,1'):
                byte_mode = (opcode == 'RES,1')
                count = self.resolve_expression(operand_str, self.current_address)
                if count is None:
                    if operand_str.upper() in self.labels:
                        count = self.labels[operand_str.upper()]
                    else:
                        count = 1
                if byte_mode:
                    self.current_address += count
                else:
                    if self.current_address % 2 == 1:
                        self.current_address += 1
                    self.current_address += count * 2
                continue
                
            if opcode == 'BND':
                if self.current_address % 2 == 1:
                    self.current_address += 1
                continue
                
            if opcode == 'TEXT':
                if operand_str.startswith('"'):
                    end_quote = operand_str.find('"', 1)
                    if end_quote != -1:
                        text = operand_str[1:end_quote]
                        i = 0
                        while i < len(text):
                            if text[i] == '\\' and i + 1 < len(text):
                                self.current_address += 1
                                i += 2
                            else:
                                self.current_address += 1
                                i += 1
                continue
                
            if opcode and opcode in self.known_opcodes and opcode not in self.directives:
                self.current_address += 2

    def second_pass(self):
        """Second pass: generate binary output."""
        self.output = bytearray()
        self.current_address = 0
        listing = []
        
        for line_num, line in self.lines:
            label, opcode, operand_str = self.parse_line(line)
            start_addr = self.current_address
            line_bytes = b''
            
            if not opcode:
                listing.append((line_num, line, start_addr, line_bytes))
                continue
                
            if opcode == 'EQU':
                listing.append((line_num, line, start_addr, line_bytes))
                continue
                
            if opcode in ('CDS', 'LDS', 'IDS', 'LPS', 'FIN', 'END', 'DEF', 'REF', 'PAGE', 'GOTO', 'DO', 'BASE'):
                if opcode in ('CDS', 'LDS', 'IDS', 'LPS', 'FIN'):
                    self.current_address = 0
                listing.append((line_num, line, start_addr, line_bytes))
                continue
                
            if opcode == 'ORG':
                val = self.resolve_expression(operand_str, self.current_address)
                if val is not None:
                    self.current_address = val
                listing.append((line_num, line, start_addr, line_bytes))
                continue
                
            code = self.assemble_instruction(line_num, opcode, operand_str)
            if code:
                line_bytes = code
                self.output += code
                self.current_address += len(code)
                
            listing.append((line_num, line, start_addr, line_bytes))
            
        return listing

    # ------------------------------------------------------------------
    # Listing mode: reads the disassembler's own output format
    #     000106:<TAB>0314<TAB>EOR <TAB>DL <TAB>V_0014<TAB>; comment
    #     NAME:<TAB>; comment            (label on its own line)
    #     NAME  EQU  #hex                (constants)
    # The text (mnemonic + mode + operand) is assembled; the hex column is
    # used (a) to fill in what the listing cannot express and (b) to verify.
    # ------------------------------------------------------------------
    LISTING_LINE_RE = re.compile(r'^([0-9A-Fa-f]{6}):[ \t]*(.*)$')
    LABEL_LINE_RE = re.compile(r'^([A-Za-z_][A-Za-z0-9_]*):\s*$')
    EQU_LINE_RE = re.compile(r'^([A-Za-z_][A-Za-z0-9_]*)\s+EQU\s+(\S+)', re.I)
    ALL_MODES = {'DL', 'DG', 'IL', 'IG', 'P', 'PX', 'ILX', 'IGX', 'RP', 'RM'}
    BRANCHES = {'BRU', 'BRX', 'BCT', 'BOT', 'BCF', 'BOF', 'BAZ', 'BAN',
                'BE', 'BZ', 'BL', 'BLZ', 'BNE', 'BNZ', 'BGE', 'BPZ'}

    @classmethod
    def is_listing_file(cls, path):
        try:
            with open(path, 'r', encoding='utf-8') as f:
                for line in f:
                    if cls.LISTING_LINE_RE.match(line):
                        return True
        except FileNotFoundError:
            pass
        return False

    def parse_listing(self):
        """Parse the listing file into self.items (structured, one per source line)."""
        self.items = []
        self.ignored_text_lines = []
        try:
            fh = open(self.source_file, 'r', encoding='utf-8')
        except FileNotFoundError:
            print(f"Error: File '{self.source_file}' not found.")
            sys.exit(1)
        with fh:
            for n, line in enumerate(fh, 1):
                line = line.rstrip('\r\n')
                s = line.strip()
                if not s or s.startswith(';'):
                    continue
                m = self.LISTING_LINE_RE.match(line)
                if m:
                    it = self._parse_listing_item(n, int(m.group(1), 16), m.group(2))
                    if it:
                        self.items.append(it)
                    continue
                code = s.split(';', 1)[0].strip()
                m = self.LABEL_LINE_RE.match(code)
                if m:
                    self.items.append({'kind': 'label', 'line': n, 'name': m.group(1)})
                    continue
                m = self.EQU_LINE_RE.match(code)
                if m:
                    self.items.append({'kind': 'equ', 'line': n,
                                       'name': m.group(1), 'expr': m.group(2)})
                    continue
                # Anything else is documentation text whose ';' was lost
                # (e.g. "LEA CHAIN2", "* Function:") - never assembly.
                self.ignored_text_lines.append(n)

    def _parse_listing_item(self, n, addr, rest):
        rest = rest.strip()
        if rest.startswith('.EBCDIC'):
            raw = None
            m = re.search(r'raw:\s*((?:[0-9A-Fa-f]{4}\s*)+)$', rest)
            if m:
                raw = bytes.fromhex(m.group(1).replace(' ', ''))
            else:
                q = re.search(r'"(.*)"', rest)
                text = q.group(1) if q else ''
                raw = text.encode('cp037')
                if len(raw) % 2:
                    raw += b'\x40'
            return {'kind': 'ebcdic', 'line': n, 'addr': addr, 'raw': raw}
        body = rest.split(';', 1)[0]
        tok = body.split()
        if len(tok) < 2:
            print(f"Warning: Line {n}: cannot parse listing line: {rest!r}")
            return None
        hexw = int(tok[0], 16) if re.fullmatch(r'[0-9A-Fa-f]{4}', tok[0]) else None
        mn = tok[1].upper()
        if mn in ('.WORD', '.BYTE', '.ZERO', '??'):
            return {'kind': 'data', 'line': n, 'addr': addr, 'hex': hexw,
                    'mn': mn, 'operand': ' '.join(tok[2:])}
        mode = tok[2].upper() if len(tok) > 2 and tok[2].upper() in self.ALL_MODES else None
        operand = ' '.join(tok[3:] if mode else tok[2:])
        return {'kind': 'insn', 'line': n, 'addr': addr, 'hex': hexw,
                'mn': mn, 'mode': mode, 'operand': operand}

    # -- expressions -----------------------------------------------------
    def _eval_term(self, term, pc):
        t = term.strip()
        if not t:
            return None
        if t[0] in '#&':                       # explicit hex
            try:
                return int(t[1:], 16)
            except ValueError:
                return None
        if t[0] == '%':                        # binary
            try:
                return int(t[1:], 2)
            except ValueError:
                return None
        if t in self.labels:
            return self.labels[t]
        if t == 'G':                           # G-relative base: offset is the number that follows
            return 0
        if t == '$':
            return pc
        if re.fullmatch(r'[0-9A-Fa-f]+', t):   # bare digits are hexadecimal
            return int(t, 16)
        return None                            # (X), X, unknown label ...

    def eval_listing_expr(self, text, pc=0):
        s = text.replace(' ', '')
        if not s:
            return None
        total = 0
        parts = re.findall(r'([+-]?)([^+-]+)', s)
        if not parts:
            return None
        for sign, term in parts:
            v = self._eval_term(term, pc)
            if v is None:
                return None
            total += -v if sign == '-' else v
        return total

    # -- encoding --------------------------------------------------------
    @staticmethod
    def _strip_operand(mode, operand):
        o = operand.strip()
        if mode in ('ILX', 'IGX', 'PX') and o.upper().endswith(',X'):
            o = o[:-2]
        o = o.lstrip('*').lstrip('=')
        return o.strip()

    def encode_listing_insn(self, it):
        """Return (word, how). how: 'text' | 'hex-operand' | None (error message in it['err'])."""
        mn, mode, pc, hexw = it['mn'], it['mode'], it['addr'], it['hex']
        raw_op = it['operand']
        hex_low = (hexw & 0xFF) if hexw is not None else None
        op = self._strip_operand(mode, raw_op)

        def fallback(high, why):
            if hex_low is not None:
                return (high << 8) | hex_low, 'hex-operand'
            it['err'] = why
            return None, None

        # --- derived shift family (operand = shift count, 0 if omitted)
        for table, highs in ((self.derived_shifts, {'DL': 0x30, 'PX': 0xE0, 'P': 0xF0}),
                             (self.derived_shc,    {'DL': 0x3C, 'PX': 0xEC, 'P': 0xFC})):
            if mn in table:
                if mode not in highs:
                    it['err'] = f"unsupported addressing mode '{mode}' for {mn}"
                    return None, None
                cnt = self.eval_listing_expr(op, pc) if op else 0
                if cnt is None or not (0 <= cnt <= 0x1F):
                    return fallback(highs[mode], f"bad shift count '{op}'")
                return (highs[mode] << 8) | (table[mn] << 5) | cnt, 'text'

        # --- derived register-operation family (XAE, CCE, ...)
        if mn in self.derived_srg:
            high = {'DL': 0x31, 'PX': 0xE1, 'P': 0xF1}.get(mode)
            if high is None:
                it['err'] = f"unsupported addressing mode '{mode}' for {mn}"
                return None, None
            return (high << 8) | (self.derived_srg[mn] << 1), 'text'

        if mn not in self.opcode_hex:
            it['err'] = f"unknown instruction '{mn}'"
            return None, None
        entry = self.opcode_hex[mn]
        if mode not in entry:
            it['err'] = f"unsupported addressing mode '{mode}' for {mn}"
            return None, None
        opc = entry[mode]
        if opc > 0xFF:                          # fixed 16-bit instruction (RTS, DIT, STM ...)
            # DITR is the SHC opcode with code 1: its 5 low (count) bits are don't-care
            # and the listing prints no operand for them -> take them from the hex column.
            if mn == 'DITR' and hexw is not None and (hexw & ~0x1F) == opc:
                return hexw, 'hex-operand'
            return opc, 'text'

        # --- relative branches: operand is a word count
        if mode in ('RP', 'RM'):
            if re.fullmatch(r'\$\+X', op, re.I):
                # BRX table jump: the offset is not derivable from '$+X' (X is a run-time value)
                if hex_low is not None:
                    return (opc << 8) | hex_low, 'hex-operand'
                target = pc                     # no hex column: assume offset 0
            else:
                target = self.eval_listing_expr(op, pc)
            if target is None:
                return fallback(opc, f"cannot resolve branch target '{op}'")
            diff = (target - pc) if mode == 'RP' else (pc - target)
            if diff < 0 or diff % 2 or diff // 2 > 0xFF:
                return fallback(opc, f"branch target '{op}' out of range "
                                     f"({target - pc:+d} bytes)")
            return (opc << 8) | (diff // 2), 'text'

        # --- everything else: operand is one byte
        val = self.eval_listing_expr(op, pc) if op else None
        if val is None or not (0 <= val <= 0xFF):
            return fallback(opc, f"cannot resolve operand '{raw_op}'")
        return (opc << 8) | val, 'text'

    def assemble_listing(self, verbose_listing=False):
        items = self.items
        # ---- first pass: labels (placed at the address of the next code line) and EQUs
        self.labels = {}
        pending = []
        for it in items:
            if it['kind'] == 'label':
                pending.append(it)
            elif 'addr' in it:
                for lb in pending:
                    if lb['name'] in self.labels and self.labels[lb['name']] != it['addr']:
                        print(f"Warning: Line {lb['line']}: label '{lb['name']}' redefined")
                    self.labels[lb['name']] = it['addr']
                pending = []
        for lb in pending:                      # labels after the last code line
            self.labels.setdefault(lb['name'], 0)
        equs = [it for it in items if it['kind'] == 'equ']
        for _ in range(4):                      # EQUs may refer to labels / other EQUs
            for e in equs:
                v = self.eval_listing_expr(e['expr'])
                if v is not None:
                    self.labels[e['name']] = v
        for e in equs:
            if e['name'] not in self.labels:
                print(f"Error: Line {e['line']}: cannot resolve EQU {e['name']} = {e['expr']}")

        # ---- second pass: code generation, placed by address
        image = bytearray()
        self.gaps = []
        self.mismatches = []
        self.errors = 0
        n_text = n_hexop = n_data = 0
        listing = []

        def place(addr, data, line):
            nonlocal image
            if addr < len(image):
                print(f"Error: Line {line}: address {addr:06X} overlaps previous output "
                      f"(image already at {len(image):06X})")
                self.errors += 1
                return
            if addr > len(image):
                self.gaps.append((len(image), addr))
                image += b'\x00' * (addr - len(image))
            image += data

        for it in items:
            k = it['kind']
            if k not in ('insn', 'data', 'ebcdic'):
                continue
            ln, addr = it['line'], it['addr']
            if k == 'ebcdic':
                data = it['raw']
                n_data += 1
            elif k == 'data':
                mn, hexw = it['mn'], it['hex']
                if mn == '.ZERO':
                    try:
                        cnt = int(it['operand'].strip(), 10)
                    except ValueError:
                        cnt = None
                    if cnt is None:
                        print(f"Error: Line {ln}: bad .ZERO count '{it['operand']}'")
                        self.errors += 1
                        cnt = 0
                    data = b'\x00\x00' * cnt
                elif mn == '??':
                    if hexw is None:
                        print(f"Error: Line {ln}: '??' without raw word")
                        self.errors += 1
                        continue
                    data = pack_mitra_word(hexw)
                elif mn == '.BYTE':
                    v = self.eval_listing_expr(it['operand'], addr)
                    data = bytes([(v if v is not None else 0) & 0xFF])
                else:                           # .WORD
                    v = self.eval_listing_expr(it['operand'], addr) if it['operand'] else None
                    if v is None:
                        v = hexw
                    if v is None:
                        print(f"Error: Line {ln}: cannot resolve .WORD '{it['operand']}'")
                        self.errors += 1
                        v = 0
                    data = pack_mitra_word(v & 0xFFFF)
                    if hexw is not None and (v & 0xFFFF) != hexw:
                        self.mismatches.append((ln, addr, v & 0xFFFF, hexw, it['operand']))
                n_data += 1
            else:                               # instruction
                word, how = self.encode_listing_insn(it)
                if word is None:
                    print(f"Error: Line {ln}: {it.get('err', 'cannot assemble')}")
                    self.errors += 1
                    if it['hex'] is not None:   # still keep the image intact
                        word = it['hex']
                    else:
                        continue
                else:
                    hexw = it['hex']
                    if hexw is not None and word != hexw:
                        if (word ^ hexw) == 0x0001:
                            word = hexw         # listing shows the word-aligned symbol; bit 0 = byte select
                        else:
                            self.mismatches.append((ln, addr, word, hexw,
                                                    f"{it['mn']} {it['mode']} {it['operand']}"))
                    if how == 'text':
                        n_text += 1
                    else:
                        n_hexop += 1
                data = pack_mitra_word(word)
            place(addr, data, ln)
            listing.append((ln, addr, data))

        self.output = image
        self.stats = (n_text, n_hexop, n_data)
        if verbose_listing:
            for ln, addr, data in listing:
                print(f"{addr:06X} | {' '.join('%02X' % b for b in data[:8]):<24} | line {ln}")
        return image

    def assemble_listing_file(self, verbose_listing=False):
        self.parse_listing()
        image = self.assemble_listing(verbose_listing)
        n_text, n_hexop, n_data = self.stats
        print("Listing-format source detected.")
        print(f"  Labels/constants defined : {len(self.labels)}")
        print(f"  Instructions assembled   : {n_text} from text, "
              f"{n_hexop} using the listing's hex byte for an operand the text cannot express "
              f"((X), $+X, ...)")
        print(f"  Data items               : {n_data} (.WORD/.ZERO/??/.EBCDIC)")
        print(f"  Documentation lines skipped (no ';'): {len(self.ignored_text_lines)}")
        if self.gaps:
            print(f"  Address gaps zero-filled : " +
                  ', '.join(f"{a:06X}-{b - 1:06X}" for a, b in self.gaps))
        if self.mismatches:
            print(f"  WARNING: {len(self.mismatches)} line(s) assemble to something other than "
                  f"the listing's hex column (text wins):")
            for ln, addr, w, h, txt in self.mismatches[:40]:
                print(f"    line {ln}: {addr:06X}  text->{w:04X}  listing={h:04X}   {txt}")
            if len(self.mismatches) > 40:
                print(f"    ... and {len(self.mismatches) - 40} more")
        else:
            print("  Verification             : output matches the listing's hex column")
        print(f"  Image size               : {len(image)} bytes (0000-{len(image) - 1:04X})")
        print(f"  Errors                   : {self.errors}")
        return image

    def assemble(self):
        """Perform the assembly process."""
        self.read_source()
        
        # First pass: collect all labels
        self.first_pass()
        
        # Print label table for debugging
        print("Labels resolved:")
        for label, addr in sorted(self.labels.items()):
            print(f"  {label}: {addr:04X}")
        print()
        
        # Second pass: generate code
        listing = self.second_pass()
        
        print("Address  | Hex Data        | Source Line")
        print("---------+-----------------+------------------")
        for line_num, line, addr, line_bytes in listing:
            if len(line_bytes) == 0:
                hex_data = "                 "
            else:
                hex_data = ' '.join('{:02X}'.format(b) for b in line_bytes)
                hex_data = f"{hex_data:<17}"
            line_display = line[:50] + "..." if len(line) > 50 else line
            print(f"{addr:08X} | {hex_data} | {line_display}")
            
        return self.output

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 mitra15_asm.py <source_file> [output_bin_file]")
        print("Example: python3 mitra15_asm.py program.asm program.bin")
        sys.exit(1)
        
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    source_file = args[0]
    output_file = args[1] if len(args) > 1 else re.sub(r'\.asm$', '', source_file) + '.bin'
    
    assembler = MITRA15Assembler(source_file)
    if MITRA15Assembler.is_listing_file(source_file):
        binary_output = assembler.assemble_listing_file('--listing' in sys.argv)
    else:
        binary_output = assembler.assemble()
    
    if binary_output:
        with open(output_file, 'wb') as f:
            f.write(binary_output)
        print(f"\nBinary output written to {output_file}")
    else:
        print("No binary output generated.")

if __name__ == "__main__":
    main()