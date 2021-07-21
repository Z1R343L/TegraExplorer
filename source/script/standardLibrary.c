#include "model.h"
#include "compat.h"
#include "genericClass.h"
#include "eval.h"
#include "garbageCollector.h"
#include "intClass.h"
#include "standardLibrary.h"
#include "scriptError.h"
#include <string.h>

#ifndef WIN32
#include "../storage/mountmanager.h"
#include "../keys/keys.h"
#include "../fs/readers/folderReader.h"
#include "../fs/fscopy.h"
#include <mem/heap.h>
#include "../keys/nca.h"
#endif

ClassFunction(stdIf) {
	s64 value = getIntValue(args[0]);

	if (value) {
		Variable_t* res = genericCallDirect(args[1], NULL, 0);
		if (res == NULL)
			return NULL;

		removePendingReference(res);
	}

	Variable_t* ret = newIntVariablePtr(value);
	ret->variableType = ElseClass;

	return ret;
}

ClassFunction(stdWhile) {
	Variable_t* result = eval(args[0]->function.function.operations.data, args[0]->function.function.operations.count, 1);
	if (result == NULL || result->variableType != IntClass)
		return NULL;

	while (result->integer.value) {
		removePendingReference(result);
		Variable_t* res = genericCallDirect(args[1], NULL, 0);
		if (res == NULL) {
			if (scriptLastError == SCRIPT_BREAK) {
				break;
			}
			else {
				return NULL;
			}
		}

		removePendingReference(res);

		result = eval(args[0]->function.function.operations.data, args[0]->function.function.operations.count, 1);
		if (result == NULL || result->variableType != IntClass)
			return NULL;
	}

	removePendingReference(result);

	return &emptyClass;
}

ClassFunction(stdPrint) {
	for (int i = 0; i < argsLen; i++) {
		Variable_t* res = callMemberFunctionDirect(args[i], "print", NULL, 0);
		if (res == NULL)
			return NULL;
	}
	

	return &emptyClass;
}

ClassFunction(stdPrintLn) {
	stdPrint(caller, args, argsLen);
	gfx_printf("\n");
	return &emptyClass;
}

ClassFunction(stdExit) {
	return NULL;
}

ClassFunction(stdBreak) {
	scriptLastError = SCRIPT_BREAK;
	return NULL;
}

ClassFunction(stdDict) {
	Variable_t a = { 0 };
	a.variableType = DictionaryClass;
	a.dictionary.vector = newVec(sizeof(Dict_t), 0);
	return copyVariableToPtr(a);
}

#ifndef WIN32
ClassFunction(stdMountSysmmc){
	if (connectMMC(MMC_CONN_EMMC))
		return newIntVariablePtr(1);

	Variable_t *arg = (*args);
	if (mountMMCPart(arg->string.value).err)
		return newIntVariablePtr(1); // Maybe change for error?

	return newIntVariablePtr(0);
}

ClassFunction(stdMountSave){
	Variable_t *arg = (*args);
	Variable_t var = {.variableType = SaveClass};
	SaveClass_t* save = calloc(1, sizeof(SaveClass_t));
	if (f_open(&save->saveFile, arg->string.value, FA_READ))
		return NULL;
	save_init(&save->saveCtx, &save->saveFile, dumpedKeys.save_mac_key, 0);
	if (!save_process(&save->saveCtx))
		return NULL;
	
	var.save = save;
	return copyVariableToPtr(var);
}
ClassFunction(stdSetPixel) {
	u32 color = getIntValue(args[2]);
	gfx_set_pixel_horz(args[0]->integer.value, args[1]->integer.value, color);
	return &emptyClass;
}

ClassFunction(stdReadDir){
	Vector_t dict = newVec(sizeof(Dict_t), 4);
	Variable_t* resPtr = newIntVariablePtr(0);
	Dict_t temp = {.name = CpyStr("result"), .var = resPtr};
	vecAdd(&dict, temp);

	Variable_t fileNamesArray = {.variableType = StringArrayClass, .solvedArray.vector = newVec(sizeof(char*), 0)};
	Variable_t dirNamesArray = {.variableType = StringArrayClass, .solvedArray.vector = newVec(sizeof(char*), 0)};
	Variable_t fileSizeArray = {.variableType = IntArrayClass, .solvedArray.vector = newVec(sizeof(s64), 0)};

	DIR dir;
	if ((resPtr->integer.value = f_opendir(&dir, args[0]->string.value))){
		Variable_t ret = {.variableType = DictionaryClass, .dictionary.vector = dict};
		return copyVariableToPtr(ret);
	}

	FILINFO fno;
	while (!(resPtr->integer.value = f_readdir(&dir, &fno)) && fno.fname[0]){
		char *name = CpyStr(fno.fname);
		if (fno.fattrib & AM_DIR){
			vecAdd(&dirNamesArray.solvedArray.vector, name);
		}
		else {
			vecAdd(&fileNamesArray.solvedArray.vector, name);
			s64 size = fno.fsize;
			vecAdd(&fileSizeArray.solvedArray.vector, size);
		}
	}

	f_closedir(&dir);

	temp.name = CpyStr("files");
	temp.var = copyVariableToPtr(fileNamesArray);
	vecAdd(&dict, temp);

	temp.name = CpyStr("folders");
	temp.var = copyVariableToPtr(dirNamesArray);
	vecAdd(&dict, temp);
	
	temp.name = CpyStr("fileSizes");
	temp.var = copyVariableToPtr(fileSizeArray);
	vecAdd(&dict, temp);

	Variable_t ret = {.variableType = DictionaryClass, .dictionary.vector = dict};
	return copyVariableToPtr(ret);
}

ClassFunction(stdFileCopy){
	ErrCode_t e = FileCopy(args[0]->string.value, args[1]->string.value, 0);
	return newIntVariablePtr(e.err);
}

ClassFunction(stdMkdir){
	return newIntVariablePtr(f_mkdir(args[0]->string.value));
}

// Broken????
ClassFunction(stdGetMemUsage){
	heap_monitor_t mon;
	heap_monitor(&mon, false);
	Dict_t a = {.name = CpyStr("used"), .var = newIntVariablePtr((s64)mon.used)};
	Dict_t b = {.name = CpyStr("total"), .var = newIntVariablePtr((s64)mon.total)};
	Variable_t ret = {.variableType = DictionaryClass, .dictionary.vector = newVec(sizeof(Dict_t), 2)};
	vecAdd(&ret, a);
	vecAdd(&ret, b);
	return copyVariableToPtr(ret);
}

ClassFunction(stdGetNcaType){
	int type = GetNcaType(args[0]->string.value);
	return newIntVariablePtr(type);
}
#else
ClassFunction(stdMountSysmmc){
	return newIntVariablePtr(0);
}
ClassFunction(stdMountSave){
	return newIntVariablePtr(0);
}
ClassFunction(stdSetPixel) {
	return newIntVariablePtr(0);
}

ClassFunction(stdReadDir){
	return newIntVariablePtr(0);
}

ClassFunction(stdFileCopy){
	return newIntVariablePtr(0);
}

ClassFunction(stdMkdir){
	return newIntVariablePtr(0);
}

ClassFunction(stdGetMemUsage) {
	return newIntVariablePtr(0);
}
#endif

u8 oneIntoneFunction[] = { IntClass, FunctionClass };
u8 doubleFunctionClass[] = { FunctionClass, FunctionClass };
u8 oneStringArgStd[] = {StringClass};
u8 threeIntsStd[] = { IntClass, IntClass, IntClass };
u8 twoStringArgStd[] = {StringClass, StringClass};

ClassFunctionTableEntry_t standardFunctionDefenitions[] = {
	{"if", stdIf, 2, oneIntoneFunction},
	{"while", stdWhile, 2, doubleFunctionClass},
	{"print", stdPrint, VARARGCOUNT, 0},
	{"println", stdPrintLn, VARARGCOUNT, 0},
	{"mountsys", stdMountSysmmc, 1, oneStringArgStd},
	{"readsave", stdMountSave, 1, oneStringArgStd},
	{"exit", stdExit, 0, 0},
	{"break", stdBreak, 0, 0},
	{"dict", stdDict, 0, 0},
	{"setpixel", stdSetPixel, 3, threeIntsStd},
	{"readdir", stdReadDir, 1, oneStringArgStd},
	{"filecopy", stdFileCopy, 2, twoStringArgStd},
	{"mkdir", stdMkdir, 1, oneStringArgStd},
	{"memory", stdGetMemUsage, 0, 0},
	{"ncatype", stdGetNcaType, 1, oneStringArgStd},
};

ClassFunctionTableEntry_t* searchStdLib(char* funcName) {
	for (int i = 0; i < ARRAY_SIZE(standardFunctionDefenitions); i++) {
		if (!strcmp(funcName, standardFunctionDefenitions[i].name))
			return &standardFunctionDefenitions[i];
	}

	return NULL;
}