#include "http_client.h"
#include <cstdio>
#include <iostream>

using namespace std;

bool CHttpClient::ms_bInit = false;

namespace {

// trim from start (in place)
void __Ltrim(string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
        return !std::isspace(ch);
    }));
}

// trim from end (in place)
void __Rtrim(string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

// trim from both ends (in place)
void __Trim(string &s) {
    __Ltrim(s);
    __Rtrim(s);
}    

}   //anonymous namespace

CHttpClient::CHttpClient(const std::string& strBaseUrl)
{
    if (!ms_bInit)
    {
        curl_global_init(CURL_GLOBAL_ALL);
        ms_bInit = true;
    }

    m_oCallbackObj.pHttpClient = this;

    m_pCurl = curl_easy_init();
    m_strBaseUrl = strBaseUrl;
    // curl_easy_setopt(m_pCurl, CURLOPT_HEADER, 1L);
    curl_easy_setopt(m_pCurl, CURLOPT_NOPROGRESS, 1L);

    //注册读返回数据
    curl_easy_setopt(m_pCurl, CURLOPT_WRITEFUNCTION, CHttpClient::__ReceiveData);
    //注册 callback data
    curl_easy_setopt(m_pCurl, CURLOPT_WRITEDATA, &m_oCallbackObj);
    //header读取
    curl_easy_setopt(m_pCurl, CURLOPT_HEADERFUNCTION, CHttpClient::__ReceiveHeader);
    curl_easy_setopt(m_pCurl, CURLOPT_HEADERDATA, &m_oCallbackObj);

}

CHttpClient::~CHttpClient()
{
    if (m_pCurl != nullptr)
    {
        curl_easy_cleanup(m_pCurl);
        m_pCurl = nullptr;
    }
}

void CHttpClient::Init()
{
    if (!ms_bInit)
    {
        curl_global_init(CURL_GLOBAL_ALL);
        ms_bInit = true;
    }
}

void CHttpClient::Cleanup()
{
    if (ms_bInit)
    {
        curl_global_cleanup();
        ms_bInit = false;
    }
}

void CHttpClient::SetVerbose(bool bFlag)
{
    long lValue = bFlag ? 1L : 0L;
    curl_easy_setopt(m_pCurl, CURLOPT_VERBOSE, lValue);
}

void CHttpClient::SetHeader(const std::string& strName, const std::string strValue)
{
    m_oRequestHeaders[strName] = strValue;
}

const ResponseData& CHttpClient::Get(const std::string& strUri)
{
    return __PerformRequest(strUri);
}

const ResponseData& CHttpClient::Post(const std::string& strUri, const std::string strData)
{
    // set post body
    curl_easy_setopt(m_pCurl, CURLOPT_POSTFIELDS, strData.c_str());
    curl_easy_setopt(m_pCurl, CURLOPT_POSTFIELDSIZE, strData.size());

    return __PerformRequest(strUri);
}

const ResponseData& CHttpClient::PostForm(const std::string& strUri, const HttpForms& forms)
{
    SetHeader("Content-Type", "application/x-www-form-urlencoded");

    //prepare form data
    std::string strData;
    int n = 0;
    for (const auto& form : forms)
    {
        if (n > 0)
        {
            strData += "&";
        }
        ++n;
        strData += __UrlEncode(form.first);
        strData += "=";
        strData += __UrlEncode(form.second);
    }

    return Post(strUri, strData);
}

const ResponseData& CHttpClient::PostJson(const std::string& strUri, const std::string& strJsonData)
{
    SetHeader("Content-Type", "application/json");
    return Post(strUri, strJsonData);
}



///============================== private part =================================

size_t CHttpClient::__ReceiveData(void *pDataBuff, size_t nSize, size_t nMemb, void *pCallbackBuff)
{
    const CallbackObj* pCallbackObj = reinterpret_cast<CallbackObj*>(pCallbackBuff);
    // assert(pCallbackBuff != nullptr);

    CHttpClient* pThisClient = pCallbackObj->pHttpClient;
    //assert(pThisClient != nullptr);
    return pThisClient->__DoReceiveData(pDataBuff, nSize, nMemb);
}

size_t CHttpClient::__ReceiveHeader(void *pDataBuff, size_t nSize, size_t nMemb, void *pCallbackBuff)
{
    const CallbackObj* pCallbackObj = reinterpret_cast<CallbackObj*>(pCallbackBuff);
    // assert(pCallbackBuff != nullptr);

    CHttpClient* pThisClient = pCallbackObj->pHttpClient;
    //assert(pThisClient != nullptr);
    return pThisClient->__DoReceiveHeader(pDataBuff, nSize, nMemb);
}

size_t CHttpClient::__DoReceiveData(void *pDataBuff, size_t nSize, size_t nMemb)
{
    //assert
    const char* pszData = reinterpret_cast<char*>(pDataBuff);
    m_oResponseData.body.append(pszData, nSize * nMemb);

    return nSize * nMemb;
}

size_t CHttpClient::__DoReceiveHeader(void *pDataBuff, size_t nSize, size_t nMemb)
{
    //每次拿到一个header即会调用本函数

    //注意不要认定pDataBuff是\0结尾的
    char* pBuff = reinterpret_cast<char*>(pDataBuff);
    std::string strTmpBuff(pBuff, nSize * nMemb);

    auto nStatus = strTmpBuff.find("HTTP/1.1");
    if (nStatus != std::string::npos)
    {
        return nSize * nMemb;
    }

    auto nPos = strTmpBuff.find_first_of(":");
    string strKey = strTmpBuff.substr(0, nPos);
    __Trim(strKey);
    if (strKey.empty())
    {
        return nSize * nMemb;
    }
    strTmpBuff.find("HTTP/");
    string strValue("");
    if (nPos != std::string::npos)
    {
        strValue = strTmpBuff.substr(nPos + 1);
    }
    __Trim(strValue);
    m_oResponseData.headers[strKey] = strValue;

    cout << "key: <" << strKey << ">, value: <" << strValue << ">\n";
    
    return nSize * nMemb;
}

std::string CHttpClient::__UrlEncode(const std::string& str)
{
	char* encoded = curl_easy_escape(m_pCurl, str.c_str() , str.length());
	std::string res(encoded);
	curl_free(encoded);
	return res;
}

const ResponseData& CHttpClient::__PerformRequest(const std::string& uri)
{
    //prepare response
    m_oResponseData.code = 0;
    m_oResponseData.body.clear();
    m_oResponseData.headers.clear();

    //url
    std::string url = m_strBaseUrl + uri;
    curl_easy_setopt(m_pCurl, CURLOPT_URL, url.c_str());

    //headers
    struct curl_slist *headers = nullptr; /* init to NULL is important */
    std::string strTemp;
    for (const auto& header : m_oRequestHeaders)
    {
        strTemp = header.first + ": " + header.second;
        headers = curl_slist_append(headers, strTemp.c_str());
    }
    if (!strTemp.empty())
    {
        curl_easy_setopt(m_pCurl, CURLOPT_HTTPHEADER, headers);
    }
    
    //TODO timeout

    //body already be prepared by caller

    // perform
    CURLcode result = curl_easy_perform(m_pCurl);
    if (result != CURLE_OK)
    {
        m_oResponseData.code = result;
        m_oResponseData.body = curl_easy_strerror(result);
    }
    else
    {
        m_oResponseData.code = 200;
    }
    
    // free headers
    curl_slist_free_all(headers);

    return m_oResponseData;
}