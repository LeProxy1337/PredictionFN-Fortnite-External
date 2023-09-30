#pragma once

namespace code_cave
{
	typedef unsigned char BYTE;

	bool initialize_code_cave()
	{
		// vmstorfl.sys Related to the VM Storage Fitler in virtual environments
		// partmgr.sys Works, Manages disk partitions
		void* target_module = utils::get_kernel_image(_("volsnap.sys"));
		if (!target_module)
			return false;

		char section_char[] = { 'I', 'N', 'I', 'T','\0' };
		cache::cave_base = utils::find_section((uintptr_t)target_module, reinterpret_cast<char*>(section_char));
		cache::cave_base = cache::cave_base - 0x30;
		cache::cave_base_2 = cache::cave_base - 0x30 + 20;

		crt::kmemset(&section_char, 0, sizeof(section_char));

		return bool(cache::cave_base);
	}
}