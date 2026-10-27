"""Single-device stereo audio service. Imported lazily; browser control is isolated."""
import time
import traceback
import numpy as np

class AudioService:
    def __init__(self, graph,sr=48000,block=512):
        self.graph=graph
        self.sr=sr
        self.block=block
        self.stream=None
        self.enabled=False
        self.duplex=False
        self.xruns=0
        self.error=''
        self._fifo=np.zeros((2,0),dtype=np.float32)

    @staticmethod
    def list_devices():
        try:
            import sounddevice as sd
            return [{'id':i,'name':d['name'],'output':d['max_output_channels'],
                     'input':d['max_input_channels'],'default_rate':d['default_samplerate']}
                    for i,d in enumerate(sd.query_devices())]
        except Exception as exc:return [{'error':str(exc)}]

    def stop(self):
        old=self.stream
        self.stream=None
        self.enabled=False
        if old:
            old.stop()
            old.close()
        self._fifo=np.zeros((2,0),np.float32)

    def start(self,device=None,duplex=False,latency='low'):
        import sounddevice as sd
        self.stop()
        self.error=''
        kwargs=dict(samplerate=self.sr,blocksize=self.block,dtype='float32',
                    latency=latency)
        if device is not None and str(device).strip():device=int(device)
        else:device=None
        # Real callback always executes audio graph; WS/control never processes audio.
        def out_only(outdata,frames,t,status):
            try:
                if status:self.xruns+=1
                block=self._process(frames,None)
                outdata[:]=block.T
            except Exception as ex:
                self.error=str(ex)
                outdata.fill(0)
                self.xruns+=1
        def full_duplex(indata,outdata,frames,t,status):
            try:
                if status:self.xruns+=1
                data=indata.T.copy() if indata.size else None
                outdata[:]=self._process(frames,data).T
            except Exception as ex:
                self.error=str(ex)
                outdata.fill(0)
                self.xruns+=1
        try:
            if duplex:
                device_pair=(None,device) if device is not None else None
                self.stream=sd.Stream(device=device_pair,channels=(2,2),callback=full_duplex,**kwargs)
            else:
                self.stream=sd.OutputStream(device=device,channels=2,callback=out_only,**kwargs)
            self.stream.start()
            self.enabled=True
            self.duplex=bool(duplex)
        except Exception as exc:
            self.error=str(exc)
            self.stop()
            raise
        return {'enabled':self.enabled,'duplex':self.duplex,'device':str(self.stream.device),
                'samplerate':self.sr,'blocksize':self.block}

    def _process(self,frames,input_audio):
        # Output streams should request a fixed BLOCK. Provide FIFO when host
        # backend unexpectedly delivers a different size; silence if needed.
        if frames==self.block:
            return self.graph.process(frames,input_audio=input_audio)
        if self.duplex:
            # Input/output frame mismatch requires exact signal alignment; don't
            # silently rearrange external input or change the synthesis timing.
            raise RuntimeError('Device supplied an unexpected frame count in duplex mode')
        while self._fifo.shape[1] < frames:
            b=self.graph.process(self.block)
            self._fifo=np.concatenate((self._fifo,b),axis=1)
        out=self._fifo[:,:frames]
        self._fifo=self._fifo[:,frames:]
        return out

    def status(self):
        return {'running':self.enabled,'duplex':self.duplex,'xruns':self.xruns,
                'error':self.error, **self.graph.report}
