#ifndef __HTTP_CLIENT_H__
#define __HTTP_CLIENT_H__

#include <unordered_map>
#include <string>
#include "curl/curl.h"

typedef std::unordered_map<std::string, std::string> HttpHeaders;
typedef std::unordered_map<std::string, std::string> HttpForms;

struct ResponseData
{
    int code;
    std::string body;
    HttpHeaders headers;
};

class CHttpClient
{
public:
    explicit CHttpClient(const std::string& strBaseUrl);
    ~CHttpClient();

    void SetVerbose(bool bFlag);
    void SetHeader(const std::string& strName, const std::string strValue);

    const ResponseData& Get(const std::string& strUri);
    const ResponseData& Post(const std::string& strUri, const std::string strData);

    const ResponseData& PostForm(const std::string& strUri, const HttpForms& forms);
    const ResponseData& PostJson(const std::string& strUri, const std::string& strJsonData);

    static void Init();
    static void Cleanup();

private:
    struct CallbackObj
    {
        CHttpClient* pHttpClient;
    };

    static size_t __ReceiveData(void *pDataBuff, size_t nSize, size_t nMemb, void *pCallbackBuff);
    static size_t __ReceiveHeader(void *pDataBuff, size_t nSize, size_t nMemb, void *pCallbackBuff);
    size_t __DoReceiveData(void *pDataBuff, size_t nSize, size_t nMemb);
    size_t __DoReceiveHeader(void *pDataBuff, size_t nSize, size_t nMemb);
    std::string __UrlEncode(const std::string& str);
    const ResponseData& __PerformRequest(const std::string& uri);

private:
    CURL* m_pCurl;
    std::string m_strBaseUrl;
    HttpHeaders m_oRequestHeaders;
    CallbackObj m_oCallbackObj;
    ResponseData m_oResponseData;

    static bool ms_bInit;
};


#endif //__HTTP_CLIENT_H__