// vim: set sw=4 et ts=4:
// Copyright (C) 2019 Samsung Electronics Co., Ltd.
// See the LICENSE file in the project root for more information.

// This file containes Tizen-specific additions to CoreCLR.

#include "common.h"
#include "vars.hpp"
#include "tizen.h"
#include "tizen_init.h"
#include <stdio.h>

extern "C" void *dlsym(void *, const char *);

namespace
{
    const unsigned PATH_MAX = 4096;
    const char *TizenFuncName = "__tizen_clr";

    struct Import
    {
        void (*const inst_class)(const char *fileName, const char *className, unsigned nArgs);

        void (*const inst_func)(const char *methodName, const void *sigData, unsigned sigLen, unsigned nArgs);
    };

    struct Export
    {
        void* (*const loadClass)(const char *fileName, const char *className, long *errCode);

        void* (*const specializeClass)(void *typeHandle, void *typeArgs, unsigned nArgs, long *errCode);

        void* (*const findFunc)(void* classHandle, const char* methodName, const void *signature, unsigned sigLength, long *errCode);

        void* (*const specializeFunc)(void* funcHandle, void* typeArgs, unsigned nArgs, long *errCode);

        HRESULT (*const compileFunc)(void *funcHandle, long *resultSize);
    };

    extern const Export export_func;


    class TizenInterface
    {
    public:
        static const Import* instance()
        {
            static TizenInterface i;
            return i._import;
        }

    private:
        const Import *_import;

        TizenInterface() : _import(nullptr)
        {
            void *addr = dlsym(NULL, TizenFuncName);
            if (!addr) return;

            _import = reinterpret_cast<const Import* (*)(const Export*)>(addr)(&export_func);
        }
    };


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


    HRESULT precompileFunc(void *classHandle, const char *methodName,
                void *typeArgs, unsigned nArgs,
                const void *signature, unsigned sigLength
    ) {
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

            TypeHandle typehandle;
            *reinterpret_cast<void**>(&typehandle) = classHandle;
            MethodDesc *method = MemberLoader::FindMethod(
                    typehandle.GetMethodTable(),
                    methodName, static_cast<const COR_SIGNATURE *>(signature), sigLength,
                    typehandle.GetModule(), MemberLoader::FM_Unique);

            if (method == nullptr)
                ThrowHR(COR_E_AMBIGUOUSMATCH);

            PCODE pCode = method->PrepareInitialCode();
        }

        END_EXTERNAL_ENTRYPOINT;
        END_ENTRYPOINT_NOTHROW;

        return hr;
    }


    const Export export_func =
    {
        loadClass,
        specializeClass,
        findFunc,
        specializeFunc,
        compileFunc
    };


    // see vm/class.cpp:1809  MethodTable::_GetFullyQualifiedNameForClassNestedAwareInternal

    // dump class information recursively
    void dumpClassInfo(MethodTable *pmt)
    {
        // Get module's file name.
        const Module *module = pmt->GetModule();
        ScratchBuffer<PATH_MAX+1> buf;
        LPCUTF8 fileName = module->GetFile()->GetPath().GetUTF8(buf);

        // Get class name.
        DefineFullyQualifiedNameForClass();
        LPCUTF8 className = GetFullyQualifiedNameForClassNestedAware(pmt);

        if (pmt->HasInstantiation())
        {
            Instantiation inst = pmt->GetInstantiation();
            unsigned ninst = inst.GetNumArgs();
            for (unsigned n = 0; n < ninst; n++)
            {
                TypeHandle th = ClassLoader::CanonicalizeGenericArg(inst[n]);
                dumpClassInfo(th.GetMethodTable());
                //SString name;
                //ScratchBuffer<PATH_MAX> sb;
                //TizenInterface::instance()->register_GetUTF8(sb);
            }

            TizenInterface::instance()->inst_class(fileName, className, ninst);
        }
        else {
            TizenInterface::instance()->inst_class(fileName, className, 0);
        }
    }

} // anonymous


namespace Tizen
{
    void Initialize()
    {
        TizenInterface::instance();
    }

    void MethodPrepared(MethodDesc *methodDesc)
    {
        if (! TizenInterface::instance())
            return;

        if (methodDesc->IsLCGMethod() || methodDesc->IsILStub())
            return;

        if (methodDesc->IsPreImplemented()) // code from native image
            return;

        PCODE pCode = methodDesc->GetNativeCode();
        if (!pCode)
            return;

        EECodeInfo codeInfo(pCode);
        if (!codeInfo.IsValid())
            return;

        // Get method name.
        LPCUTF8 methodName = methodDesc->GetName(methodDesc->GetSlot());


        // Get method's signature
        Signature sig = methodDesc->GetSignature();
        const unsigned char *sigData = sig.GetRawSig();
        unsigned sigLen = sig.GetRawSigLen();

        unsigned ninst = 0;
        if (methodDesc->HasMethodInstantiation())
        {
            Instantiation inst = methodDesc->GetMethodInstantiation();
            ninst = inst.GetNumArgs();
            for (unsigned n = 0; n < ninst; n++)
            {
                TypeHandle th = ClassLoader::CanonicalizeGenericArg(inst[n]);
                dumpClassInfo(th.GetMethodTable());
            }
        }

        dumpClassInfo(methodDesc->GetMethodTable());
        TizenInterface::instance()->inst_func(methodName, sigData, sigLen, ninst);
    }

} // Tizen namespace
