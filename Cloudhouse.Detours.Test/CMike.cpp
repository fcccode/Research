#include "stdafx.h"
#include "CMike.h"

CMike::CMike()
  : _refCount(1)
{
}

HRESULT STDMETHODCALLTYPE CMike::QueryInterface(/* [in] */ REFIID riid, /* [iid_is][out] */ _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject)
{
  auto result = E_NOINTERFACE;

  *ppvObject = nullptr;

  if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IMike))
  {
    *ppvObject = this;

    AddRef();

    result = S_OK;
  }

  return result;
}

ULONG STDMETHODCALLTYPE CMike::AddRef()
{
    return InterlockedIncrement(&_refCount);
}

ULONG STDMETHODCALLTYPE CMike::Release()
{
  auto const count = InterlockedDecrement(&_refCount);

  if (count == 0)
  {
    delete this;
  }

  return count;
}
