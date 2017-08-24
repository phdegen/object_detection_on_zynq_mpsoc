import numpy as np

import cv2

img = np.fromfile('img_out.bin', 'uint8').reshape(375,1242,3)


cv2.imshow('image',img)

cv2.waitKey(6000)
