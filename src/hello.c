#include "redux_api.h"

int _start(const redux_api_t *api)
{
    if (api == 0)
    {
        return 1;
    }

    if (api->version != REDUX_API_VERSION)
    {
        return 2;
    }

    api->println("Hello World!");
    api->println(
        "This is the very first Redux Program BTW!"
    );

    return 0;
}
