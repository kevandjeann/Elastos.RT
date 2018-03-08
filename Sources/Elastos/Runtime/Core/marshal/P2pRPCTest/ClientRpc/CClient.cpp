
#include "CClient.h"
#include "Carrier.h"
#include <pthread.h>
#include <sys/time.h>
#include <stdlib.h>
#include "Base64.h"

#define SERVER_ADDRESS "6in5GLjxmfmsMWZB1ttFCAGXwmJi3orTVWPogmv44ZvXsyuQwRR6"
#define SERVER_UID "3bx3rqkQ6RpNjqi51GaD1DqsVXU5jGxtv27c3CsH7chN"

CAR_OBJECT_IMPL(CClient)

CAR_INTERFACE_IMPL(CClient, Object, IClient);


CClient::~CClient()
{
    if (mCarrier != NULL) {
        carrier_destroy();
        mCarrier = NULL;
    }
}

ECode CClient::AddFriend(
    /* [in] */ const String& address)
{
    //TODO: hello replace with uid
    int ret = carrier_add_friend(mCarrier, address.string(), "hello");
    if (ret != 0) {
        RPC_LOG("Add friend failed %d\n", ret);
        return E_FAIL;
    }

    ECode ec = NOERROR;
    ret = carrier_wait(30);
    if (ret != 0) {
        RPC_LOG("watting for friend accept failed %d\n", ret);
        return E_FAIL;
    }

    DataPack data;
    ret = carrier_read(&data);
    if (ret != 0) {
        RPC_LOG("AddFriend read data failed %d\n", ret);
        return E_FAIL;
    }

    void* p = data.data->GetPayload();
    int type = *(size_t *)p;
    ArrayOf<Byte>::Free(data.data);
    if (type == ADD_FRIEND_SUCCEEDED && !strcmp(data.from.string(), SERVER_UID)) {
        ec = NOERROR;
    } else {
        ec = E_FAIL;
    }

    return ec;
}

ECode CClient::SendMessage(
    /* [in] */ const String& to,
    /* [in] */ Int32 type,
    /* [in] */ const String& msg)
{
    printf("Send message to %s content: %s\n", to.string(), msg.string());
    int ret = carrier_send(mCarrier, to.string(), type, msg.GetBytes()->GetPayload(), msg.GetByteLength());
    if (ret != 0) {
        RPC_LOG("Send message failed %d\n", ret);
        return E_FAIL;
    }


    return NOERROR;
}

ECode CClient::GetService(
    /* [in] */ const String& name,
    /* [out] */ InterfacePack* ip)
{
    ECode ec = SendMessage(String(SERVER_UID), GET_SERVICE, name);
    if (FAILED(ec)) return ec;

    int ret = carrier_wait(30);
    if (ret != 0) {
        RPC_LOG("Watting for get service message failed %d\n", ret);
        return E_FAIL;
    }

    DataPack data;
    ret = carrier_read(&data);
    void* p = data.data->GetPayload();
    int type = *(size_t *)p;
    RPC_LOG("GetService receive type:%d\n", type);
    if (type != GET_SERVICE_REPLY) {
        ec = E_FAIL;
        goto exit;   
    }

    p += 4;
    ec = *(ECode *)p;
    RPC_LOG("GetService receive ec:%x\n", ec);
    if (SUCCEEDED(ec)) {
        p += 4;
        int size = *(int *)p;
        p += 4;
        memcpy(ip, p, sizeof(InterfacePack));
    }

exit:
    ArrayOf<Byte>::Free(data.data);
    return ec;
}

ECode CClient::GetService(
    /* [in] */ const String& name,
    /* [out] */ IInterface ** ppService)
{
    if (!ppService) {
        return E_INVALID_ARGUMENT;
    }

    Boolean isFriend = carrier_is_friend(mCarrier, SERVER_UID);

    ECode ec = NOERROR;
    if (!isFriend) {
        ec = AddFriend(String(SERVER_ADDRESS));
        if (FAILED(ec)) return ec;
    }

    ElaConnectionStatus status = carrier_get_friend_status(SERVER_UID);
    if (status != ElaConnectionStatus_Connected) {
        RPC_LOG("Watting for friend online\n");

        int ret = carrier_wait(30);
        if (ret != 0) {
            RPC_LOG("Watting for friend online failed %d\n", ret);
            return E_FAIL;
        }

        DataPack data;
        ret = carrier_read(&data);
        if (ret != 0) {
            RPC_LOG("Watting for friend online read data failed %d\n", ret);
            return E_FAIL;
        }

        void* p = data.data->GetPayload();
        int type = *(size_t *)p;
        ArrayOf<Byte>::Free(data.data);
        if (type == FRIEND_ONLINE && !strcmp(data.from.string(), SERVER_UID)) {
            ec = NOERROR;
        } else {
            ec = E_FAIL;
        }
        if (FAILED(ec)) return ec;
    }


    InterfacePack ip;
    ec = GetService(name, &ip);
    if (FAILED(ec)) return ec;

    printf("service %s stubConnName is %s\n", name.string(), ip.m_stubConnName);
    Int32 size;
    ec = _CObject_UnmarshalInterface((void*)&ip, MarshalType_RPC,
        UnmarshalFlag_Noncoexisting, ppService, &size);
    return ec;
}

ECode CClient::constructor()
{
    return carrier_connect("/home/zuo/work/Client", &mCarrier);
}


