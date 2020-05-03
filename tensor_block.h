#ifndef __TENSOR_BLOCK_H__
#define __TENSOR_BLOCK_H__

#include <string>

class TensorBlock
{
public:
    TensorBlock(std::string startOp, std::string endOp, unsigned long offset, unsigned long size)
    {
        this->startOp = startOp;
        this->endOp = endOp;
        this->offset = offset;
        this->size = size;
    }

    std::string startOp, endOp;
    unsigned long offset, size;
};

#endif  // __TRACE_SESSION_H__