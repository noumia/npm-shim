/*
zdl.h
*/

#pragma once

#include <windows.h>
#include <stdio.h>
#include <string.h>

/* */

struct str_buffer {
	WCHAR* m;
	size_t s;
	WCHAR* p;
};

typedef struct str_buffer str_buffer_t;

WCHAR* str_alloc(str_buffer_t* t, size_t s);

void str_free(str_buffer_t* t);

WCHAR* str_realloc(str_buffer_t* t, size_t n);

WCHAR* str_append(str_buffer_t* t, const WCHAR* s, const WCHAR* e);

WCHAR* str_append_sz(str_buffer_t* t, const WCHAR* s);
WCHAR* str_append_s(str_buffer_t* t, const str_buffer_t* s);

WCHAR* str_GetModuleFileName(str_buffer_t* t);

WCHAR* str_GetEnvironmentVariable(str_buffer_t* t, const WCHAR* name);

/* */

struct path_context {
	str_buffer_t path;

	const WCHAR* base_s;
	const WCHAR* base_e;

	const WCHAR* name_s;
	const WCHAR* name_e;
};

typedef struct path_context path_context_t;

int path_setup(path_context_t* t);

void path_free(path_context_t* t);

/* */

struct run_context {
	path_context_t path;
	str_buffer_t   command;
	const WCHAR*   args;
	HANDLE         process;
};

typedef struct run_context run_context_t;

int run_setup(run_context_t* t, size_t s);

int run_launch(run_context_t* t, int* code);

void run_free(run_context_t* t);

/* */

#ifdef ZDL__IMPL_

static const WCHAR* lookup_tail(const WCHAR* args, const WCHAR* end);

WCHAR* str_alloc(str_buffer_t* t, size_t s)
{
	WCHAR* m = (WCHAR*) malloc(sizeof(WCHAR) * s);
	if (m == NULL) {
		t->m = t->p = NULL;
		t->s = 0;
		return NULL;
	}

	if (s > 0) {
		m[0] = L'\0';
	}

	t->m = t->p = m;
	t->s = s;

	return t->m;
}

void str_free(str_buffer_t* t)
{
	free(t->m);

	t->m = t->p = NULL;
	t->s = 0;
}

WCHAR* str_realloc(str_buffer_t* t, size_t n)
{
	size_t c = t->p - t->m;

	if (c + n + 1 > t->s) {
		WCHAR* m = t->m;
		size_t s = t->s;

		while (c + n + 1 > s) {
			if (s * sizeof(WCHAR) < t->s * sizeof(WCHAR)) {
				return NULL;
			}
			s *= 2;
		}

		m = (WCHAR*) realloc(m, s * sizeof(WCHAR));
		if (m == NULL) {
			return NULL;
		}

		t->m = m;
		t->p = m + c;
		t->s = s;
	}

	return t->m;
}

WCHAR* str_append(str_buffer_t* t, const WCHAR* s, const WCHAR* e)
{
	size_t n = e - s;

	if (str_realloc(t, n) == NULL) {
		return NULL;
	}

	memcpy(t->p, s, n * sizeof(WCHAR));

	t->p   += n;
	t->p[0] = L'\0';

	return t->m;
}

WCHAR* str_append_sz(str_buffer_t* t, const WCHAR* s)
{
	return str_append(t, s, s + wcslen(s));
}

WCHAR* str_append_s(str_buffer_t* t, const str_buffer_t* s)
{
	return str_append(t, s->m, s->p);
}

WCHAR* str_GetModuleFileName(str_buffer_t* t)
{
	if (str_alloc(t, MAX_PATH) == NULL) {
		return NULL;
	}

	for (; ; ) {
		t->m[t->s - 1] = L'\0';

		DWORD r = GetModuleFileNameW(NULL, t->m, (DWORD) (t->s - 1));
		if (r == 0) {
			return NULL;
		}

		if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
			t->p = t->m + r;
			break;
		}

		if (str_realloc(t, r) == NULL) {
			return NULL;
		}
	}

	return t->m;
}

WCHAR* str_GetEnvironmentVariable(str_buffer_t* t, const WCHAR* name)
{
	if (str_alloc(t, 128) == NULL) {
		return NULL;
	}

	for (; ; ) {
		DWORD r = GetEnvironmentVariableW(name, t->m, (DWORD) t->s);
		if (r == 0) {
			return NULL;
		}

		if (r + 1 <= t->s) {
			t->p = t->m + r;
			break;
		}

		if (str_realloc(t, r) == NULL) {
			return NULL;
		}
	}

	return t->m;
}

/* */

int path_setup(path_context_t* t)
{
	WCHAR* p;
	WCHAR* s;
	WCHAR* e;

	memset(t, 0, sizeof(path_context_t));

	p = str_GetModuleFileName(&(t->path));
	if (p == NULL) {
		return 0;
	}

	s = p;
	e = wcsrchr(s, L'\\');
	if (e == NULL) {
		e = s + wcslen(s);
	}

	t->base_s = s;
	t->base_e = e;

	p = e;
	if (p[0] == L'\\') {
		p += 1;
	}

	s = p;
	e = wcsrchr(s, L'.');
	if (e == NULL) {
		e = s + wcslen(s);
	}

	t->name_s = s;
	t->name_e = e;

	return 1;
}

void path_free(path_context_t* t)
{
	str_free(&(t->path));
}

static const WCHAR* lookup_tail(const WCHAR* args, const WCHAR* end)
{
	int flag = 0;

	for (const WCHAR* p = args; p < end; p++) {
		WCHAR ch = *p;

		switch (flag) {
		case 0:
			if (ch == L' ') {
				flag = 2;
			} else if (ch == L'\"') {
				flag = 1;
			}
			break;

		case 1:
			if (ch == L'\"') {
				flag = 0;
			}
			break;

		case 2:
			if (ch != L' ') {
				return p;
			}
			break;
		}
	}

	return end;
}

/* */

int run_setup(run_context_t* t, size_t s)
{
	const WCHAR* args = GetCommandLineW();

	memset(t, 0, sizeof(run_context_t));

	if (!path_setup(&(t->path))) {
		return 0;
	}

	t->args = lookup_tail(args, args + wcslen(args));

	if (str_alloc(&(t->command), s) == NULL) {
		return 0;
	}

	return 1;
}

static int run_wait(run_context_t* t, int* code)
{
	if (WaitForSingleObject(t->process, INFINITE) != WAIT_OBJECT_0) {
		return 0;

	} else {
		DWORD r;
		if (!GetExitCodeProcess(t->process, &r)) {
			return 0;
		}

		*code = (int) r;
	}

	return 1;
}

int run_launch(run_context_t* t, int* code)
{
	STARTUPINFOW        si = { 0 };
	PROCESS_INFORMATION pi = { 0 };

	WCHAR* cmd = t->command.m;

	si.cb = sizeof(si);

	*code = 1;

	if (!CreateProcessW(
		NULL,
		cmd,
		NULL,
		NULL,
		TRUE,
		0,
		NULL,
		NULL,
		&si,
		&pi)) {
		return 0;
	}

	CloseHandle(pi.hThread);

	t->process = pi.hProcess;

	return run_wait(t, code);
}

#ifndef SEE_MASK_NOCLOSEPROCESS
#define SEE_MASK_NOCLOSEPROCESS 0x40

typedef struct _SHELLEXECUTEINFOW {
	DWORD cbSize;
	ULONG fMask;
	HWND hwnd;
	LPCWSTR lpVerb;
	LPCWSTR lpFile;
	LPCWSTR lpParameters;
	LPCWSTR lpDirectory;
	int nShow;
	HINSTANCE hInstApp;
	void *lpIDList;
	LPCWSTR lpClass;
	HKEY hkeyClass;
	DWORD dwHotKey;
	HANDLE hIcon;
	HANDLE hProcess;
} SHELLEXECUTEINFOW,*LPSHELLEXECUTEINFOW;

#endif

typedef BOOL (WINAPI * ShellExecuteExW_t)(LPSHELLEXECUTEINFOW lpExecInfo);

int run_shell(run_context_t* t, const WCHAR* verb, int* code)
{
	SHELLEXECUTEINFOW sei = { 0 };

	ShellExecuteExW_t pfn = (ShellExecuteExW_t)
		GetProcAddress(LoadLibraryW(L"shell32.dll"), "ShellExecuteExW");

	sei.cbSize = (DWORD) sizeof(sei);
	sei.fMask  = SEE_MASK_NOCLOSEPROCESS;
	sei.lpVerb = verb;
	sei.lpFile = t->command.m;
	sei.nShow  = SW_SHOWNORMAL;

	*code = 1;

	if (pfn == NULL || !pfn(&sei)) {
		return 0;
	}

	t->process = sei.hProcess;

	return run_wait(t, code);
}

void run_free(run_context_t* t)
{
	if (t->process != NULL) {
		CloseHandle(t->process);
		t->process = NULL;
	}

	str_free(&(t->command));
	path_free(&(t->path));
}

#endif

/* */

