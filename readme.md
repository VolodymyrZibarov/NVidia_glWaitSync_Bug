This is a minimal reproducible example for a possible bug in NVidia drivers
This is two quadro cards with mosaic configured.
We rendering on a fullscreen window. 
We have parallel openGL context to upload textures using PBOs.
We have glWaitSync() in render pipeline to wait for texture to be uplaoded in a parallel context
We expect that both GPU will render same pipeline, but it looks like secondary GPU starts to render texture that has old data in it
The video of a problem:
https://youtu.be/aNtiaH2vPq0


