#include "seq/exc.h"

using namespace seq::exc;

SeqException::SeqException(std::string msg) : std::runtime_error(msg)
{
}

IOException::IOException(std::string msg) : std::runtime_error(msg)
{
}
