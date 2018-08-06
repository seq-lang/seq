#include "seq/stage.h"
#include "seq/exc.h"

using namespace seq;

exc::SeqException::SeqException(std::string msg) : std::runtime_error(msg)
{
}

exc::StageException::StageException(std::string msg, Stage &stage) :
    std::runtime_error("(stage: " + stage.getName() + ") " + msg)
{
}

exc::ValidationException::ValidationException(Stage &stage) :
    StageException("validation failed", stage)
{
}

exc::IOException::IOException(std::string msg) : std::runtime_error(msg)
{
}
