import os

with open('qemu.log', 'r') as f:
    lines = f.readlines()

for line in lines:
    if 'at 0x' in line:
        strs = line.split('at ')
        addr = strs[1].strip()
        cmd = 'addr2line -C -f -p -e build/kernel ' + addr
        # execute the command and get the output
        output = os.popen(cmd).read()
        output = output.replace("/mnt/ucore/ccore/", "")
        line = strs[0] + f'at ({addr})' + output
    
    print(line, end='')
