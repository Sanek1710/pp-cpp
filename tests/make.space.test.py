#!/usr/bin/python3

import subprocess
from itertools import islice

exps = [ 
  "-",
  "--",
  "-=",
  "->",
  ",",
  ";",
  ":",
  "::",
  ":>",
  "!",
  "!=",
  "?",
  ".",
  "...",
  "(",
  ")",
  "[",
  "]",
  "{",
  "}",
  "*",
  "*=",
  "/",
  "/=",
  "&",
  "&&",
  "&=",
  # "#",
  # "##",
  "%",
  # "%:",
  # "%:%:",
  "%=",
  "%>",
  "^",
  "^=",
  "+",
  "++",
  "+=",
  "<",
  "<:",
  "<%",
  "<<",
  "<<=",
  "<=",
  "=",
  "==",
  ">",
  ">=",
  ">>",
  ">>=",
  "|",
  "|=",
  "||",
  "~",
  "a",
  "0",
  "\"\""
]

defines = [
  f"#define M{i}() {exp}"
  for i, exp in enumerate(exps)
]

uses = [
  f"M{i}()M{j}()"
  for i, _ in enumerate(exps)
  for j, _ in enumerate(exps)
]

in_fname = "input/spacing.cpp"

with open(in_fname, "w") as fin:
  fin.write('\n'.join(defines))
  fin.write('\n')
  fin.write('\n'.join(uses))

pc = subprocess.Popen(["g++", "-E", in_fname], 
                      stdout=subprocess.PIPE)
stdout, stderr = pc.communicate()

outries = stdout.decode().split('\n')[7:-1]

chunks = []
it = iter(outries)
while chunk := list(islice(it, len(exps))):
  chunks.append(chunk)
chunks.pop(-1)

exp_fname = "exp/spacing.cpp"
with open(exp_fname, "w") as out:
  out.write('\n'*len(exps))
  out.write('\n'.join(outries))

for i, chunk in enumerate(chunks):
  for j, pair in enumerate(chunk):
    df = len(pair) - len(exps[i]) - len(exps[j])
    c = ' '
    if df > 0: c = '+'
    elif df < 0: c = '-'
    # if c == '+':
    #   print(pair)
    print("{:1}".format(c), end="|")
    # print(pair, exps[i], exps[j])
  print() 
print()