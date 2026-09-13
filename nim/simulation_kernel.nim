type EcologyKernel* = ref object
  width,height,count*:int
  x,y,energy:seq[int32]
  direction:seq[uint8]

proc createEcologyKernel*(count,width,height:int):EcologyKernel {.exportc.} =
  result=EcologyKernel(width:width,height:height,count:count,x:newSeq[int32](count),y:newSeq[int32](count),energy:newSeq[int32](count),direction:newSeq[uint8](count))
  for index in 0..<count:
    result.x[index]=int32(index mod width)
    result.y[index]=int32((index div width) mod height)
    result.energy[index]=12000
    result.direction[index]=uint8(index and 3)

proc stepEcologyKernel*(kernel:EcologyKernel,ticks:int) {.exportc.} =
  for tick in 0..<ticks:
    for index in 0..<kernel.count:
      case kernel.direction[index]
      of 0: kernel.y[index]=max(0'i32,kernel.y[index]-1)
      of 1: kernel.y[index]=min(int32(kernel.height-1),kernel.y[index]+1)
      of 2: kernel.x[index]=max(0'i32,kernel.x[index]-1)
      else: kernel.x[index]=min(int32(kernel.width-1),kernel.x[index]+1)
      kernel.energy[index]=max(0'i32,kernel.energy[index]-17)

proc kernelChecksum*(kernel:EcologyKernel):int32 {.exportc.} =
  for index in 0..<kernel.count: result=result+kernel.x[index]+kernel.y[index]+kernel.energy[index]

{.emit: "export { createEcologyKernel, stepEcologyKernel, kernelChecksum };".}
