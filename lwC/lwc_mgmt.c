#include <errno.h>
#include <string.h>
#include <stdint.h>

#include "lwc_sandbox.h"



static int process_elf_ph(struct dune_elf *elf, Elf64_Phdr *phdr) {
	int ret;
	int perm = PERM_U;
	off_t off;

	if (phdr->p_type == PT_INTERP) {
		log_err("lwc_sandbox: .interp sections are not yet supported.\n");
		return -EINVAL;
	}

	if (phdr->p_type != PT_LOAD)
		return 0; // continue to next section.

	log_info("lwc_sandbox info: loading segment -va 0x%09lx, len %lx\n",
		phdr->p_vaddr, phdr->p_memsz);

	if (elf->hdr.e_type == ET_DYN)
		off = LOADER_VADDR_OFF;
	else 
		off = 0;

	if (phdr->p_vaddr + off + phdr->p_memsz > MEM_IX_BASE_ADDR ||
		phdr->p_memsz > MEM_IX_BASE_ADDR || // for overflow
		phdr->p_filesz > phdr->p_memsz) {
		log_err("lwc_sandbox: segment address is insecure.\n");
		return -EINVAL;
	}

	//TODO might want to change this.
	ret = dune_vm_map_phys(pgroot,
		(void *)(phdr->p_vaddr + off),
		phdr->p_memsz,
		(void *)(phdr->p_vaddr + off),
		PERM_R | PERM_W);

	if (ret) {
		log_err("lwc_sandbox: segment mapping failed.\n");
		return ret;
	}

	ret = dune_elf_load_ph(elf, phdr, off);
	if (ret) {
		log_err("lwc_sandbox: segment load failed.\n");
		return ret;
	}

	if (phdr->p_flags & PF_X)
		perm |= PERM_X;
	if (phdr->p_flags & PF_R)
		perm |= PERM_R;
	if (phdr->p_flags & PF_W)
		perm |= PERM_W;

	ret = dune_vm_mprotect(pgroot, (void*) (phdr->p_vaddr + off),
		phdr->p_memsz, perm);

	if (ret) {
		log_err("lwc_sandbox: segment protection failed.\n");
		return ret;
	}

	return 0;
}




static int load_elf(const char *path, struct elf_data *data) {
	int ret;
	struct dune_elf elf;

	ret = dune_elf_open(&elf, path);
	if (ret)
		return ret;

	if (elf.hdr.e_type != ET_EXEC &&
		elf.hdr.e_type != ET_DYN) {
		log_err("lwc_sandbox: ELF header is not a valid type.\n");
		ret = -EINVAL;
		goto out; 
	}

	ret = dune_elf_iter_ph(&elf, &process_elf_ph);
	if (ret)
		goto out;

	if (elf.hdr.e_type == ET_DYN) {
		data->entry = (uintptr_t)(elf.hdr.e_entry + LOADER_VADDR_OFF);
		data->phdr = (Elf64_Phdr *)(elf.hdr.e_phoff + LOADER_VADDR_OFF);
	} else {
		data->entry = (uintptr_t) elf.hdr.e_entry;
		// FIXME: could probably make this more robust.
		data->phdr = (Elf64_Phdr *)(elf.hdr.e_phoff + 0x400000);
	}
	data->phnum = elf.hdr.e_phnum;

out:
	dune_elf_close(&elf);
	return ret;
}