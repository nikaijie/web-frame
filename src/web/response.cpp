#include "../../include/web/response.h"

namespace web {

std::string Response::ToString() const {
    return body_.str();
}

}
