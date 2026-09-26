#include "status_hal.hpp"

// An explicit switch, not a cast: the two enums agree today only by coincidence.
Status fromHAL(HAL_StatusTypeDef halStatus)
{
    switch (halStatus)
    {
        case HAL_OK:      return Status::OK;
        case HAL_BUSY:    return Status::BUSY;
        case HAL_TIMEOUT: return Status::TIMEOUT;
        case HAL_ERROR:   return Status::ERROR;
    }
    return Status::ERROR;
}
