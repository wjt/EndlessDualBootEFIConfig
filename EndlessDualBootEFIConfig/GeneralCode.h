#pragma once

extern FILE *logFile;

#define uprintf(format, ...) \
	do { \
		printf(format, __VA_ARGS__); \
		if (logFile != NULL) \
		fprintf(logFile, format, __VA_ARGS__); \
	} while (0)

#define uwprintf(format, ...) \
	do { \
		wprintf(format, __VA_ARGS__); \
		if (logFile != NULL) \
			fwprintf(logFile, format, __VA_ARGS__); \
	} while (0)

#define PRINT_ERROR_MSG(__ERROR_MSG__) uprintf("%s:%d %s (GLE=[%d])\n",__FILE__, __LINE__, __ERROR_MSG__, GetLastError())

#define IFFALSE_PRINTERROR(__CONDITION__, __ERRROR_MSG__) if(!(__CONDITION__)) { if(strlen(__ERRROR_MSG__)) PRINT_ERROR_MSG(__ERRROR_MSG__); }
#define IFFALSE_GOTOERROR(__CONDITION__, __ERRROR_MSG__) if(!(__CONDITION__)) { if(strlen(__ERRROR_MSG__)) PRINT_ERROR_MSG(__ERRROR_MSG__); goto error; }
#define IFFALSE_GOTO(__CONDITION__, __ERRROR_MSG__, __LABEL__) if(!(__CONDITION__)) { PRINT_ERROR_MSG(__ERRROR_MSG__); goto __LABEL__; }
#define IFFALSE_RETURN(__CONDITION__, __ERRROR_MSG__) if(!(__CONDITION__)) { PRINT_ERROR_MSG(__ERRROR_MSG__); return; }
#define IFFALSE_RETURN_VALUE(__CONDITION__, __ERRROR_MSG__, __RET__) if(!(__CONDITION__)) { PRINT_ERROR_MSG(__ERRROR_MSG__); return __RET__; }
#define IFFALSE_BREAK(__CONDITION__, __ERRROR_MSG__) if(!(__CONDITION__)) { PRINT_ERROR_MSG(__ERRROR_MSG__); break; }
#define IFFALSE_CONTINUE(__CONDITION__, __ERRROR_MSG__) if(!(__CONDITION__)) { PRINT_ERROR_MSG(__ERRROR_MSG__); continue; }

#define IFTRUE_GOTO(__CONDITION__, __ERRROR_MSG__, __LABEL__) if((__CONDITION__)) { PRINT_ERROR_MSG(__ERRROR_MSG__); goto __LABEL__; }
#define IFTRUE_GOTOERROR(__CONDITION__, __ERRROR_MSG__) if((__CONDITION__)) { PRINT_ERROR_MSG(__ERRROR_MSG__); goto error; }
#define IFTRUE_RETURN(__CONDITION__, __ERRROR_MSG__) if((__CONDITION__)) { PRINT_ERROR_MSG(__ERRROR_MSG__); return; }
#define IFTRUE_RETURN_VALUE(__CONDITION__, __ERRROR_MSG__, __RET__) if((__CONDITION__)) { PRINT_ERROR_MSG(__ERRROR_MSG__); return __RET__; }

#define IFFAILED_PRINTERROR(hr, msg)  { HRESULT __hr__ = (hr); if (FAILED(__hr__)) { PRINT_HRESULT(__hr__, msg); } }
#define IFFAILED_GOTO(hr, msg, label) { HRESULT __hr__ = (hr); if (FAILED(__hr__)) { PRINT_HRESULT(__hr__, msg); goto label; } }
#define IFFAILED_BREAK(hr, msg)       { HRESULT __hr__ = (hr); if (FAILED(__hr__)) { PRINT_HRESULT(__hr__, msg); break; } }
#define IFFAILED_CONTINUE(hr, msg)    { HRESULT __hr__ = (hr); if (FAILED(__hr__)) { PRINT_HRESULT(__hr__, msg); continue; } }
#define IFFAILED_GOTOERROR(hr, msg)   IFFAILED_GOTO(hr, msg, error)
#define IFFAILED_RETURN(hr, msg)            { HRESULT __hr__ = (hr); if (FAILED(__hr__)) { PRINT_HRESULT(__hr__, msg); return; } }
#define IFFAILED_RETURN_VALUE(hr, msg, ret) { HRESULT __hr__ = (hr); if (FAILED(__hr__)) { PRINT_HRESULT(__hr__, msg); return ret; } }
#define IFFAILED_RETURN_RES(hr, msg)        { HRESULT __hr__ = (hr); if (FAILED(__hr__)) { PRINT_HRESULT(__hr__, msg); return __hr__; } }

#define safe_closefile(__file__) if (__file__ != NULL) { fclose(__file__); __file__ = NULL; }
#define safe_closehandle(h) do {if ((h != INVALID_HANDLE_VALUE) && (h != NULL)) {CloseHandle(h); h = INVALID_HANDLE_VALUE;}} while(0)

#define FUNCTION_ENTER uprintf("%s:%d %s\n", __FILE__, __LINE__, __FUNCTION__)
#define FUNCTION_ENTER_FMT(fmt, ...) uprintf("%s:%d %s " fmt "\n", __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)

