"""Minimal deterministic test engine used only by tools/verify.py."""
import numpy as np
class FakeEngine:
    def __init__(self,sr):self.state={};self.notes=set()
    def set_many(self,v):self.state.update(v)
    def note_on(self,n):self.notes.add(n)
    def note_off(self,n):self.notes.discard(n)
    def panic(self):self.notes.clear()
    def render(self,n,input_block=None):
        amp=0.14+float(self.state.get('BREAK',0))*0.15 if self.notes else 0
        return np.full((2,n),amp,np.float32)
