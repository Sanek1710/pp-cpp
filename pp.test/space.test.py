import subprocess
from itertools import islice

exps = [ 
  #  "", " ", "!", "\"\"", "$", "%", "&", "'c'",
  # "(", ")", "*", "+", ",", "-", ".", "/", "0",
  # ":", ";", "<", 
  "=", 
  # ">", "?", "@", "A", "[",
  # "]", "^", "_", "`", "a", "{", "|", "}", 
  "~",
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

in_fname = "pp.test/space.test.cpp"

with open(in_fname, "w") as fin:
  fin.write('\n'.join(defines))
  fin.write('\n')
  fin.write('\n'.join(uses))

pc = subprocess.Popen(["g++", "-E", in_fname], 
                      stdout=subprocess.PIPE)
stdout, stderr = pc.communicate()

outries = stdout.decode().split('\n')[7:]

chunks = []
it = iter(outries)
while chunk := list(islice(it, len(exps))):
  chunks.append(chunk)
chunks.pop(-1)



for i, chunk in enumerate(chunks):
  for j, pair in enumerate(chunk):
    df = len(pair) - len(exps[i]) - len(exps[j])
    c = ' '
    if df > 0: c = '+'
    elif df < 0: c = '-'
    if c == '+':
      print(pair)
    # print("{:1}".format(c), end="|")
    print(pair, exps[i], exps[j])
  print() 
print()