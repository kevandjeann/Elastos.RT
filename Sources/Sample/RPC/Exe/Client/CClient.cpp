
#include "CClient.h"

#define FRIEND_ADDRESS ""
#define FRIEND_UID ""

CAR_INTERFACE_IMPL(CCarrierListener, Object, ICarrierListener)

ECode CCarrierListener::OnConnectionChanged(
    /* [in] */ Boolean online)
{
    CLIENT_LOG("=== Client connection change to: %s\n", online ? "online" : "offline");
    if (online) {
        pthread_mutex_lock(&mClient->mLock);
        mClient->mOnline = TRUE;
        pthread_mutex_unlock(&mClient->mLock);
        pthread_cond_signal(&mClient->mCv);
    }
    return NOERROR;
}

ECode CCarrierListener::OnReady()
{
    CLIENT_LOG("=== Client OnReady \n");
    return NOERROR;
}

ECode CCarrierListener::OnFriendRequest(
    /* [in] */ const String& uid,
    /* [in] */ const String& hello)
{
    CLIENT_LOG("=== friend %s request \n", uid.string());
    return NOERROR;
}

ECode CCarrierListener::OnFriendConnetionChanged(
    /* [in] */ const String& uid,
    /* [in] */ Boolean online)
{
    CLIENT_LOG("=== friend %s connection change to: %s\n", uid.string(), online ? "online" : "offline");
    if (online) {
        pthread_mutex_lock(&mClient->mLock);
        mClient->mFriendOnline = TRUE;
        pthread_mutex_unlock(&mClient->mLock);
        pthread_cond_signal(&mClient->mCv);
    }
    return NOERROR;
}

CAR_OBJECT_IMPL(CClient)

CAR_INTERFACE_IMPL(CClient, Object, IClient);

CClient::~CClient()
{
    if (mListener) {
        mCarrier->RemoveCarrierNodeListener(mListener);
        mListener->Release();
    }

    if (mCarrier) {
        mCarrier->Stop();
        mCarrier->Release();
    }

    if (mServiceManager) {
        mServiceManager->Release();
    }

    pthread_mutex_destroy(&mLock);
    pthread_cond_destroy(&mCv);
}

ECode CClient::GetService(
    /* [in] */ const String& name,
    /* [out] */ IInterface ** ppService)
{
    pthread_mutex_lock(&mLock);
    while (!mOnline) {
        pthread_cond_wait(&mCv, &mLock);
    }
    pthread_mutex_unlock(&mLock);

    IFriend* pFriend;
    ECode ec = mCarrier->GetFriend(String(FRIEND_UID), &pFriend);
    if (FAILED(ec)) {
        mCarrier->AddFriend(String(FRIEND_ADDRESS), String("hello"));
    }
    else {
        pFriend->Release();
    }

    pthread_mutex_lock(&mLock);
    while (!mFriendOnline) {
        pthread_cond_wait(&mCv, &mLock);
    }
    pthread_mutex_unlock(&mLock);

    return mServiceManager->GetService(String(FRIEND_UID), name, ppService);
}

ECode CClient::constructor(
    /* [in] */ const String& path)
{
    pthread_mutexattr_t recursiveAttr;
    pthread_mutexattr_init(&recursiveAttr);
    pthread_mutexattr_settype(&recursiveAttr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&mLock, &recursiveAttr);
    pthread_mutexattr_destroy(&recursiveAttr);

    pthread_cond_init(&mCv, NULL);

    ECode ec = _CCarrier_GetInstance(&mCarrier);
    if (FAILED(ec)) return ec;

    mListener = new CCarrierListener(this);
    if (!mListener) {
        return E_OUT_OF_MEMORY;
    }

    mListener->AddRef();
    mCarrier->AddCarrierNodeListener(mListener);

    ec = mCarrier->Start(path);
    if (FAILED(ec)) return ec;

    ec = _CServiceManager_AcquireInstance(&mServiceManager);
    return ec;
}


