#define ZDL__IMPL_

#include "zdl.h"

int main(void)
{
	int code = -1;

	run_context_t ctx;

	str_buffer_t* cmd = &(ctx.command);

	if (!run_setup(&ctx, 80)) {
		goto error;
	}

	if (str_append_sz(cmd, L"node.exe") == NULL) {
		goto error;
	}

	/* */

	if (ctx.path.base_s < ctx.path.base_e) {
		str_append_sz(cmd, L" \"");
		str_append   (cmd, ctx.path.base_s, ctx.path.base_e);
		str_append_sz(cmd, L"\\node_modules\\npm\\bin\\npm-cli.js\"");
	}

	if (ctx.args != NULL) {
		size_t len = wcslen(ctx.args);
		if (len > 0) {
			str_append_sz(cmd, L" ");
			str_append   (cmd, ctx.args, ctx.args + len);
		}
	}

	/* */

	if (!run_launch(&ctx, &code)) {
		DWORD le = GetLastError();
		fwprintf(stderr, L"error: %d\n", le);
		fwprintf(stderr, L"[%s]\n",      cmd->m);
		goto error;
	}

error:

	run_free(&ctx);

	return code;
}

