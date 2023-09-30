typedef enum _requests
{
	invoke_unique,
	invoke_start,
	invoke_base,
	invoke_context,
	invoke_read,
	invoke_write,
	invoke_mouse,
	invoke_init,
	invoke_allocate,
	invoke_protect,
	invoke_free,
	invoke_swap,
	invoke_query,
	invoke_scan,
	invoke_translate,
	invoke_dtb
} requests, * prequests;

typedef struct _MOUCLASS_INPUT_INJECTION_MANAGER {
	HANDLE MousePnpNotificationHandle;
	POINTER_ALIGNMENT ERESOURCE Resource;
	_Guarded_by_(Resource) PMOUSE_DEVICE_STACK_CONTEXT DeviceStackContext;
} MOUCLASS_INPUT_INJECTION_MANAGER, * PMOUCLASS_INPUT_INJECTION_MANAGER;

typedef struct _base_invoke {
	uint32_t pid;
	uintptr_t handle;
	const char* name;
	size_t size;
} base_invoke, * pbase_invoke;

typedef struct _context_invoke {
	DWORD pid;
	HANDLE context;
} context_invoke, * pcontext_invoke;

typedef struct _read_invoke {
	uint32_t pid;
	uintptr_t address;
	uintptr_t dtb;
	void* buffer;
	size_t size;
} read_invoke, * pread_invoke;

typedef struct _write_invoke {
	uint32_t pid;
	uintptr_t address;
	uintptr_t dtb;
	void* buffer;
	size_t size;
} write_invoke, * pwrite_invoke;

typedef struct _init_invoke {
	int count = 0;
} init_invoke, * pinit_invoke;

typedef struct _mouse_invoke {
	uint32_t pid;
	USHORT IndicatorFlags;
	LONG MovementX;
	LONG MovementY;
	ULONG PacketsConsumed;
} mouse_invoke, * pmouse_invoke;

typedef struct _allocate_invoke {
	uint32_t pid;
	uintptr_t address;
	size_t size;
	DWORD protection;
	int type;
} allocate_invoke, * pallocate_invoke;

typedef struct _protect_invoke {
	uint32_t pid;
	uintptr_t address;
	size_t size;
	DWORD protection;
	DWORD old_protection;
} protect_invoke, * pprotect_invoke;

typedef struct _free_invoke {
	uint32_t pid;
	uintptr_t address;
	size_t size;
	ULONG type;
} free_invoke, * pfree_invoke;

typedef struct _swap_invoke {
	uint32_t pid;
	uintptr_t address;
	uintptr_t address2;
	uintptr_t og_pointer;
} swap_invoke, * pswap_invoke;

typedef struct _query_invoke {
	uint32_t pid;
	uintptr_t address;
	uintptr_t address_2;
	ULONG protect;
	size_t mem_size;
} query_invoke, * pquery_invoke;

typedef struct _scan_invoke {
	uint32_t pid;
	uintptr_t module_base;
	uintptr_t address;
	SIZE_T size;
	const char* signature;
} scan_invoke, * pscan_invoke;

typedef struct _translate_invoke {
	uintptr_t virtual_address;
	uintptr_t directory_base;
	void* physical_address;
} translate_invoke, * ptranslate_invoke;

typedef struct _dtb_invoke {
	uint32_t pid;
	uintptr_t dtb;
} dtb_invoke, * pdtb_invoke;

typedef struct _invoke_data
{
	uint32_t unique;
	requests code;
	void* data;
}invoke_data, * pinvoke_data;