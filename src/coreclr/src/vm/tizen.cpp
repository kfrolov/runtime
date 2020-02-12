// vim: set sw=4 et ts=4:
// Copyright (C) 2019 Samsung Electronics Co., Ltd.
// See the LICENSE file in the project root for more information.

// This file containes Tizen-specific additions to CoreCLR.

#include "common.h"
#include "vars.hpp"
#include "tizen.h"
#include "tizen_init.h"
#include <stdio.h>

namespace
{
    // limits.h is POSIX-specific thing
    const unsigned PATH_MAX = 4096;

    // This symbol should be defined in already preloaded any external library or
    // in the executable itself: if this symbol is defined, it will be used as
    // function pointer with prototype "const Import* (const Export*)".  Such
    // function will be called only once to pass to external library "Export"
    // interface (see below) and to obtain from it "Import" interface. These
    // interfaces might be used later as described below.
    // If this symbol isn't defined in any external library: no any actions
    // will be performed.
    const char *TizenFuncName = "__tizen_clr";

    // This interface defines two function, which should be defined in other
    // loaded library or in the executable itself. These functions allow to
    // record which funcions from managed code was JIT-compiled during program
    // execution. Actually both of described below functions dumps data suitable
    // for interpreting by stack-machine (like Forth-interpreter): as a result
    // each call some amount of data is consumed (`nArgs' arguments) from the
    // stack and new data is pushed back. `inst_class' pushes new class and
    // consumes information of classes on which next class is depending
    // (for generic classes, due to specialization).
    // `inst_func' only consumes information of classes on which the function
    // is dependent. Resulting stack balance should be zero.
    //
    // In general these functions required to record which functions was
    // JIT-compiled during program execution.
    //
    struct Import
    {
        // This function is called by CoreCLR after JIT-compiling of any methods:
        // it informs, that class with `className', loaded from module `fileName',
        // is required for compiling of some generic function, for which `inst_func'
        // will be called later. This class might be dependent (for generic classes)
        // on `nArgs' other classes (for each of which `inst_class' function was
        // called just before this call). This function is recursive for generic
        // classes.
        //
        void (*const inst_class)(const char *fileName, const char *className, unsigned nArgs);

        // This function is called by CoreCLR after JIT-compiling of any methods:
        // it informs, that method `methodName', which has signature `sigData'
        // with length of `sigLen' was compiled now. For generic functions `nArgs'
        // specifies the number of classes on which this function depends. For each
        // of such class `inst_class' function should be called just before calling
        // `inst_func' function. Also for any function, ever for non-generic ones,
        // `inst_class' should be called before to specify to which class compiled
        // function belongs.
        //
        void (*const inst_func)(const char *methodName, const void *sigData, unsigned sigLen, unsigned nArgs);
    };

    // This interface defines few functions, which might be called by other loaded
    // library or the executable itself. These functions allow loading of specified
    // classes, specializing generic classes (by other previously loaded classes),
    // specializing generic functions, finding function by it's name and signature
    // (and obtainning it's handle), JIT-compiling the function.
    //
    // In general all of these function required to JIT-compile some particular
    // functions in advance, before the program start. For this dependend classes
    // should be loaded, generics specialized, etc...
    //
    struct Export
    {
        // This functions loads class `className' from module `fileName' and returns
        // some abstract handle of the class. The handle has no any special meaning,
        // except of the fact, that handles for different classes should be different
        // (when compared by value) and value of NULL is not valid handle. Handles
        // should be valid during program execution and might be passed to `specializeClass'
        // or `findFunc' functions.
        //
        // In case of error NULL will be returned and `errCode' (if it is not NULL) will be
        // assigned with code specifying the reason of the error.
        //
        void* (*const loadClass)(const char *fileName, const char *className, long *errCode);

        // This functions specializes generic class with specified types (classes).
        // The class which is specialized and all classes on which it depends should
        // be previously loaded with `loadClass' function and should be passed as
        // `typeHandle' (for generic class itself) and handles array `typeArgs' of
        // `nArgs' size (dependencies). Function returns handle of the specialized class.
        //
        // In case of error NULL is returned and `errCode' (if it is not NULL) will be
        // assigned appropriately.
        //
        void* (*const specializeClass)(void *typeHandle, void *typeArgs, unsigned nArgs, long *errCode);

        // This function allows to locate the function with name `methodName,
        // with signature `signature' of length `sigLength' in already loaded
        // class determined by handle `classHandle'. Return value is the handle
        // of the found function (class method). "Function handle" has no special
        // meaning, except of value NULL is reserved for invalid handle.
        // Functions handles should be valid during program execution and might
        // be passed to `specializeFunc' and `compileFunc' functions.
        //
        // In case of error NULL is returned and `errCode' (if not NULL) set appropriately.
        //
        void* (*const findFunc)(void* classHandle, const char* methodName, const void *signature, unsigned sigLength, long *errCode);

        // This functions specializes generic function determined by function
        // handle `funcHandle' with classes defined by type handles array `typeArgs'
        // of length `nArgs'. Result is the function handle of the specialized
        // function, which might be then used to call to `compileFunc'.
        //
        // In case of error NULL is returned and `errCode' (if not NULL) is set apporpiately.
        //
        void* (*const specializeFunc)(void* funcHandle, void* typeArgs, unsigned nArgs, long *errCode);

        // This function JIT-compiles functions determined by function handle
        // `funcHandle' (which should be obtained by calls to `fincFunc' and
        // `specializeFunc'). If `resultSize' argument isn't NULL, is will be
        // written by the size of machine code.
        //
        // In case of error result is distinct from S_OK.
        //
        HRESULT (*const compileFunc)(void *funcHandle, long *resultSize);
    };

    // forward declaration of exported functions list
    extern const Export export_func;


    // Singleton class, which is instantiated at time when CoreCLR is fully
    // initialized. This class is used to exchange interfaces with external
    // (relative to CoreCLR) module.
    class TizenInterface
    {
    public:
        // function returns address of `Import' interface (see definition above).
        static const Import* instance()
        {
            static TizenInterface i;
            return i._import;
        }

    private:
        const Import *_import;

        TizenInterface() : _import(nullptr)
        {
            #if 0
            extern "C" void *dlsym(void *, const char *);
            void *addr = dlsym(NULL, TizenFuncName);
            if (!addr) return;

            #else

            unsigned count = 1024;
            HMODULE modules[count];
            HMODULE *modptr = modules;
            NewHolder<HMODULE> modholder;

            DWORD needed;
            HANDLE process = GetCurrentProcess();

            if (!EnumProcessModules(process, modptr, sizeof(HMODULE) * count, &needed))
                return;

            while (needed > sizeof(HMODULE) * count)
            {
                modholder = modptr = static_cast<HMODULE*>(operator new(needed));
                count = needed / sizeof(HMODULE);

                if (!EnumProcessModules(process, modptr, sizeof(HMODULE) * count, &needed))
                    return;
            }

            void *addr = NULL;
            for (HMODULE *curmod = modptr;  addr != NULL && curmod < &modptr[count]; ++curmod)
                addr = reinterpret_cast<void*>(GetProcAddress(curmod, TizenFuncName));

            #endif

            _import = reinterpret_cast<const Import* (*)(const Export*)>(addr)(&export_func);
        }
    };

    // This functions loads class `className' from module `fileName' and returns
    // some abstract handle of the class. The handle has no any special meaning,
    // except of the fact, that handles for different classes should be different
    // (when compared by value) and value of NULL is not valid handle. Handles
    // should be valid during program execution and might be passed to `specializeClass'
    // or `findFunc' functions.
    //
    // In case of error NULL will be returned and `errCode' (if it is not NULL) will be
    // assigned with code specifying the reason of the error.
    //
    void* loadClass(const char *fileName, const char *className, long *errCode)
    {
        TypeHandle typehandle;

        CONTRACTL
        {
            NOTHROW;
            if (GetThread()) {GC_TRIGGERS;} else {DISABLED(GC_NOTRIGGER);}
            ENTRY_POINT;
        }
        CONTRACTL_END;

        HRESULT hr = S_OK;
        BEGIN_ENTRYPOINT_NOTHROW;
        BEGIN_EXTERNAL_ENTRYPOINT(&hr);
        GCX_COOP_THREAD_EXISTS(GET_THREAD());

        {
            GCX_PREEMP();

            AssemblySpec spec;
            spec.Init(fileName);
            Assembly *assembly = spec.LoadAssembly(FILE_ACTIVE);

            // special case: nested class
            LPCUTF8 p = strchr(className, '+');
            if (p == nullptr)
            {
                typehandle = assembly->GetLoader()->LoadTypeByNameThrowing(assembly, nullptr, className);
            }
            else {
                // load enclosing class
                COUNT_T size = p - className;
                const UTF8 *utf8 = className;
                SString name(SString::Utf8, utf8, size);
                //fprintf(stderr, "BASE: '%s' for '%s'\n", name.GetUTF8NoConvert(), className);
                TypeHandle enc_th = assembly->GetLoader()->LoadTypeByNameThrowing(assembly, nullptr, name.GetUTF8NoConvert());

                IMDInternalImport *pMDI = enc_th.GetModule()->GetMDImport();

                // load inner classes
                #if 0
                // create class name
                LPCUTF8 inner = p + 1;
                p = strchr(inner, '+');
                if (p != nullptr)
                {
                    name.SetUTF8(inner, p - inner);
                    inner = name.GetUTF8NoConvert();
                }
                #endif

                // iterate over known classes and find requested
                HENUMTypeDefInternalHolder hEnum(pMDI);
                mdTypeDef td;
                hEnum.EnumTypeDefInit();

                while (pMDI->EnumNext(&hEnum, &td))
                {
                    TypeHandle th;
                    hr = S_OK;

                    EX_TRY
                    {
                        th = assembly->GetLoader()->LoadTypeDefThrowing(enc_th.GetModule(), td);
                    }
                    EX_CATCH
                    {
                        hr = GET_EXCEPTION()->GetHR();
                    }
                    EX_END_CATCH(SwallowAllExceptions);

                    if (SUCCEEDED(hr))
                    {
                        DefineFullyQualifiedNameForClass();
                        LPCUTF8 tname = GetFullyQualifiedNameForClassNestedAware(th.GetMethodTable());
                        //fprintf(stderr, "  '%s'\n", tname);
                        if (!strcmp(tname, className))
                        {
                            typehandle = th;
                            break;  // match
                        }
                    }
                }

                if (typehandle.IsNull())
                    hr = S_FALSE;
            }
        }

        END_EXTERNAL_ENTRYPOINT;
        END_ENTRYPOINT_NOTHROW;

        if (errCode != nullptr)
            *errCode = hr;

        if (hr != S_OK)
            return nullptr;

        static_assert(sizeof(TypeHandle) == sizeof(void*), "sizeof(TypeHandle) != sizeof(void*)");
        return *reinterpret_cast<void**>(&typehandle);
    }


    // This functions specializes generic class with specified types (classes).
    // The class which is specialized and all classes on which it depends should
    // be previously loaded with `loadClass' function and should be passed as
    // `typeHandle' (for generic class itself) and handles array `typeArgs' of
    // `nArgs' size (dependencies). Function returns handle of the specialized class.
    //
    // In case of error NULL is returned and `errCode' (if it is not NULL) will be
    // assigned appropriately.
    //
    void* specializeClass(void *typeHandle, void *typeArgs, unsigned nArgs, long *errCode)
    {
        TypeHandle result;

        CONTRACTL
        {
            NOTHROW;
            if (GetThread()) {GC_TRIGGERS;} else {DISABLED(GC_NOTRIGGER);}
            ENTRY_POINT;
        }
        CONTRACTL_END;

        HRESULT hr = S_OK;
        BEGIN_ENTRYPOINT_NOTHROW;
        BEGIN_EXTERNAL_ENTRYPOINT(&hr);
        GCX_COOP_THREAD_EXISTS(GET_THREAD());

        {
            GCX_PREEMP();

            TypeHandle th;
            *reinterpret_cast<void**>(&th) = typeHandle;

            Instantiation inst(static_cast<TypeHandle*>(typeArgs), nArgs);
            result = th.Instantiate(inst);
        }


        END_EXTERNAL_ENTRYPOINT;
        END_ENTRYPOINT_NOTHROW;

        if (errCode != nullptr)
            *errCode = hr;

        if (hr != S_OK)
            return nullptr;

        return *reinterpret_cast<void**>(&result);
    }


    // This function allows to locate the function with name `methodName,
    // with signature `signature' of length `sigLength' in already loaded
    // class determined by handle `classHandle'. Return value is the handle
    // of the found function (class method). "Function handle" has no special
    // meaning, except of value NULL is reserved for invalid handle.
    // Functions handles should be valid during program execution and might
    // be passed to `specializeFunc' and `compileFunc' functions.
    //
    // In case of error NULL is returned and `errCode' (if not NULL) set appropriately.
    //
    void* findFunc(void* classHandle, const char* methodName, const void *signature, unsigned sigLength, long *errCode)
    {
        MethodDesc *result;

        CONTRACTL
        {
            NOTHROW;
            if (GetThread()) {GC_TRIGGERS;} else {DISABLED(GC_NOTRIGGER);}
            ENTRY_POINT;
        }
        CONTRACTL_END;

        HRESULT hr = S_OK;
        BEGIN_ENTRYPOINT_NOTHROW;
        BEGIN_EXTERNAL_ENTRYPOINT(&hr);
        GCX_COOP_THREAD_EXISTS(GET_THREAD());

        {
            GCX_PREEMP();

            TypeHandle th;
            *reinterpret_cast<void**>(&th) = classHandle;

            result = MemberLoader::FindMethod(
                    th.GetMethodTable(),
                    methodName, static_cast<const COR_SIGNATURE *>(signature), sigLength,
                    th.GetModule(), MemberLoader::FM_Unique);

            if (result == nullptr)
                asm("nop");

            asm("nop");
            asm("nop");
            asm("nop");

            //if (result == nullptr)
            //    ThrowHR(COR_E_AMBIGUOUSMATCH);
        }

        END_EXTERNAL_ENTRYPOINT;
        END_ENTRYPOINT_NOTHROW;

        if (errCode != nullptr)
            *errCode = hr;

        if (hr != S_OK)
            return nullptr;

        return *reinterpret_cast<void**>(&result);
    }


    // This functions specializes generic function determined by function
    // handle `funcHandle' with classes defined by type handles array `typeArgs'
    // of length `nArgs'. Result is the function handle of the specialized
    // function, which might be then used to call to `compileFunc'.
    //
    // In case of error NULL is returned and `errCode' (if not NULL) is set apporpiately.
    //
    void* specializeFunc(void* funcHandle, void* typeArgs, unsigned nArgs, long *errCode)
    {
        MethodDesc *result;

        CONTRACTL
        {
            NOTHROW;
            if (GetThread()) {GC_TRIGGERS;} else {DISABLED(GC_NOTRIGGER);}
            ENTRY_POINT;
        }
        CONTRACTL_END;

        HRESULT hr = S_OK;
        BEGIN_ENTRYPOINT_NOTHROW;
        BEGIN_EXTERNAL_ENTRYPOINT(&hr);
        GCX_COOP_THREAD_EXISTS(GET_THREAD());

        {
            GCX_PREEMP();

            MethodDesc *generic = static_cast<MethodDesc*>(funcHandle);

            Instantiation inst(static_cast<TypeHandle*>(typeArgs), nArgs);

            result = MethodDesc::FindOrCreateAssociatedMethodDesc(
                    generic, generic->GetMethodTable(),
                    FALSE,  // forceBoxedEntryPoint
                    inst,
                    TRUE   // allowInstParam
                    );
        }

        END_EXTERNAL_ENTRYPOINT;
        END_ENTRYPOINT_NOTHROW;

        if (errCode != nullptr)
            *errCode = hr;

        if (hr != S_OK)
            return nullptr;

        return result;
    }


    // see also vm/ceeload.cpp:14045 (ExpandAll)

    // This function JIT-compiles functions determined by function handle
    // `funcHandle' (which should be obtained by calls to `fincFunc' and
    // `specializeFunc'). If `resultSize' argument isn't NULL, is will be
    // written by the size of machine code.
    //
    // In case of error result is distinct from S_OK.
    //
    HRESULT compileFunc(void *funcHandle, long *resultSize)
    {
        CONTRACTL
        {
            NOTHROW;
            if (GetThread()) {GC_TRIGGERS;} else {DISABLED(GC_NOTRIGGER);}
            ENTRY_POINT;
        }
        CONTRACTL_END;

        HRESULT hr = S_OK;
        BEGIN_ENTRYPOINT_NOTHROW;
        BEGIN_EXTERNAL_ENTRYPOINT(&hr);
        GCX_COOP_THREAD_EXISTS(GET_THREAD());

        {
            GCX_PREEMP();

            MethodDesc *method = static_cast<MethodDesc*>(funcHandle);
            if (method->GetNativeCode() == 0)
            {
                if (method->HasNativeCodeSlot() || !method->HasPrecode() || method->IsDefaultInterfaceMethod())
                {
                    NativeCodeVersion codever(method);
                    // codever.SetOptimizationTier(NativeCodeVersion::OptimizationTier1);
                    PrepareCodeConfig config(codever, TRUE, TRUE);
                    //config.SetJitSwitchedToOptimized();
                    PCODE pCode = method->PrepareCode(&config);
                    if (!pCode) {
                        hr = S_FALSE;
                    }
                    else {
                        EECodeInfo codeInfo(pCode);
                        if (!codeInfo.IsValid()) {
                            hr = S_FALSE;
                        }
                        else if (resultSize != nullptr)
                        {
                            TADDR codeSize = codeInfo.GetCodeManager()->GetFunctionSize(codeInfo.GetGCInfoToken());
                            *resultSize = codeSize;
                        }
                    }
                }
                else hr = S_FALSE;
            }
        }

        END_EXTERNAL_ENTRYPOINT;
        END_ENTRYPOINT_NOTHROW;

        return hr;
    }


    // Exported functions list
    const Export export_func =
    {
        loadClass,
        specializeClass,
        findFunc,
        specializeFunc,
        compileFunc
    };


    // see vm/class.cpp:1809  MethodTable::_GetFullyQualifiedNameForClassNestedAwareInternal

    // This function records class information recursively (including all classes
    // on which current instanation of generic class is depends, because of specialization).
    void dumpClassInfo(MethodTable *pmt)
    {
        // Get module's file name.
        const Module *module = pmt->GetModule();
        ScratchBuffer<PATH_MAX+1> buf;
        LPCUTF8 fileName = module->GetFile()->GetPath().GetUTF8(buf);

        // Get class name (fully qualified, including namespaces and outer classes).
        DefineFullyQualifiedNameForClass();
        LPCUTF8 className = GetFullyQualifiedNameForClassNestedAware(pmt);

        if (pmt->HasInstantiation())
        {
            // Process dependencies for generic classes.
            Instantiation inst = pmt->GetInstantiation();
            unsigned ninst = inst.GetNumArgs();
            for (unsigned n = 0; n < ninst; n++)
            {
                TypeHandle th = ClassLoader::CanonicalizeGenericArg(inst[n]);
                dumpClassInfo(th.GetMethodTable());
            }

            TizenInterface::instance()->inst_class(fileName, className, ninst);
        }
        else {
            // Regular (non generic) class.
            TizenInterface::instance()->inst_class(fileName, className, 0);
        }
    }

} // anonymous


namespace Tizen
{
    // This function is callsed from `coreclr_initialize', after initialization
    // of the CoreCLR is completely performed.
    void Initialize()
    {
        TizenInterface::instance();
    }

    // This function is called from src/vm/prestub.cpp, after some particular
    // class method is JIT-compiled.  The task of this function is to call
    // `inst_class' and `inst_func' from `struct Import' in right order, to make
    // possible to record which classes is loaded, how generic classes and functions
    // should be specialized, and which function is compiled now.
    void MethodPrepared(MethodDesc *methodDesc)
    {
        // is some external library or main process defines `__tizen_clr` function?
        if (! TizenInterface::instance())
            return;

        // this function types can't be later (pre)compiled
        if (methodDesc->IsLCGMethod() || methodDesc->IsILStub())
            return;

        // code from native image, can't be (pre)compiled
        if (methodDesc->IsPreImplemented())
            return;

        // no native code produced: function can't be (pre)compiled
        PCODE pCode = methodDesc->GetNativeCode();
        if (!pCode)
            return;

        EECodeInfo codeInfo(pCode);
        if (!codeInfo.IsValid())
            return;

        // Get method name (only method name, not includes outer class names).
        LPCUTF8 methodName = methodDesc->GetName(methodDesc->GetSlot());

        // Get method's signature (as BLOB).
        Signature sig = methodDesc->GetSignature();
        const unsigned char *sigData = sig.GetRawSig();
        unsigned sigLen = sig.GetRawSigLen();

        unsigned ninst = 0;
        if (methodDesc->HasMethodInstantiation())
        {
            // Record information about classes on which this class depends
            // (for generic methods).
            Instantiation inst = methodDesc->GetMethodInstantiation();
            ninst = inst.GetNumArgs();
            for (unsigned n = 0; n < ninst; n++)
            {
                TypeHandle th = ClassLoader::CanonicalizeGenericArg(inst[n]);
                dumpClassInfo(th.GetMethodTable());
            }
        }

        // Record information about class to which function belongs.
        dumpClassInfo(methodDesc->GetMethodTable());

        // Record information about the function itself.
        TizenInterface::instance()->inst_func(methodName, sigData, sigLen, ninst);
    }

} // Tizen namespace
