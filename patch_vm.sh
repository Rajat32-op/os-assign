sed -i 's/if (mem == 0) return 0;/if (mem == 0) { printf("vmfault kalloc fail\\n"); return 0; }/g' kernel/vm.c
sed -i 's/if (frames_full()) return 0;/if (frames_full()) { printf("vmfault frames_full fail\\n"); return 0; }/g' kernel/vm.c
