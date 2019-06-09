#include <cstdio>
#include "http_client.h"

int main(int argc, char* argv[])
{
    CHttpClient hc("www.baidu.com");
    const auto& response = hc.Get("/");
    // printf("response: %d, %s\n", response.code, response.body.c_str());
    return 0;
}