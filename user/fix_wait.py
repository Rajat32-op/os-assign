import os

files = ['user/testraid0.c', 'user/testraid1.c', 'user/testraid5.c']

old_code = """    int cpu_evict = -1, io_evict = -1;
    wait(&cpu_evict);
    wait(&io_evict);"""

new_code = """    int cpu_evict = -1, io_evict = -1;
    for(int i = 0; i < 2; i++) {
        int status = -1;
        int pid = wait(&status);
        if(pid == pid_cpu) cpu_evict = status;
        if(pid == pid_io) io_evict = status;
    }"""

for f in files:
    with open(f, 'r') as file:
        content = file.read()
    
    content = content.replace(old_code, new_code)
    
    with open(f, 'w') as file:
        file.write(content)
