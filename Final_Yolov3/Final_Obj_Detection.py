import cv2
import numpy as np
import serial
from datetime import datetime
import time

# Allowed classes to detect
allowed_classes = {'APPLE', 'BANANA', 'ORANGE'}

# --- YOLO Configuration ---
whT = 320
confThreshold = 0.5
nmsThreshold = 0.3  

classesFile = 'coco.names'
with open(classesFile, 'rt') as f:
    classNames = f.read().rstrip('\n').split('\n')

modelConfig = 'yolov3.cfg'
modelWeights = 'yolov3.weights'
net = cv2.dnn.readNetFromDarknet(modelConfig, modelWeights)
net.setPreferableBackend(cv2.dnn.DNN_BACKEND_OPENCV)
net.setPreferableTarget(cv2.dnn.DNN_TARGET_CPU)

# --- Serial Setup ---
serialIns = serial.Serial()
serialIns.baudrate = 9600
serialIns.port = 'COM8'  # Change this to your correct COM port
serialIns.open()

log_file = open("detections.txt", "a")
already_logged = set()
prevCommand = 'nothing'

def findObject(outputs, img, prevCommand):
    hT, wT, _ = img.shape
    boxes, classIds, confs = [], [], []
    new_detections = []
    command = "nothing"

    for output in outputs:
        for det in output:
            scores = det[5:]
            cid = np.argmax(scores)
            conf = scores[cid]
            label = classNames[cid].upper()
            if conf > confThreshold and label in allowed_classes:
                w, h = int(det[2]*wT), int(det[3]*hT)
                x, y = int(det[0]*wT - w/2), int(det[1]*hT - h/2)
                boxes.append([x, y, w, h])
                classIds.append(cid)
                confs.append(float(conf))

    idxs = cv2.dnn.NMSBoxes(boxes, confs, confThreshold, nmsThreshold)

    if len(idxs) > 0:
        for i in idxs.flatten():
            x, y, w, h = boxes[i]
            label = classNames[classIds[i]].upper()
            confidence = int(confs[i] * 100)

            cv2.rectangle(img, (x, y), (x + w, y + h), (255, 0, 255), 2)
            cv2.putText(img, f'{label} {confidence}%', (x, y - 10),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 0, 255), 2)

            if label not in already_logged:
                new_detections.append(f"{label} ({confidence}%)")
                already_logged.add(label)

            command = label.lower()  # lowercase to match Arduino expectations

    # Logging
    if new_detections:
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        log_file.write(f"[{timestamp}] New detections:\n")
        for item in new_detections:
            log_file.write(f"  {item}\n")
        log_file.flush()

    # Send to Arduino only if command changed and it's valid
    if command != prevCommand and command != 'nothing':
        print("Sending to Arduino:", command)
        serialIns.write((command + '\n').encode('utf-8'))
        # cv2.waitKey(3000)  # Wait 3 sec
        time.sleep(6)

    return command

# --- Webcam as Video Source ---
cap = cv2.VideoCapture(0)

while True:
    success, frame = cap.read()
    if not success:
        print("Failed to capture frame. Exiting...")
        break

    blob = cv2.dnn.blobFromImage(frame, 1/255, (whT, whT), [0, 0, 0], swapRB=True, crop=False)
    net.setInput(blob)
    layer_names = net.getLayerNames()
    out_names = [layer_names[i - 1] for i in net.getUnconnectedOutLayers().flatten()]
    outputs = net.forward(out_names)

    prevCommand = findObject(outputs, frame, prevCommand)

    cv2.imshow('Object Detection View', frame)

    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

# Cleanup
log_file.close()
serialIns.close()
cap.release()
cv2.destroyAllWindows()