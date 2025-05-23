# Human Activity Recognition Based on ESP-DL

This is a human activity recognition model built using the [ESP-DL](https://github.com/espressif/esp-dl) framework, leveraging the [Human Activity Recognition with Smartphones Dataset](https://www.kaggle.com/datasets/uciml/human-activity-recognition-with-smartphones). The dataset was collected from 30 volunteers (aged 19-48) performing six activities (WALKING, WALKING_UPSTAIRS, WALKING_DOWNSTAIRS, SITTING, STANDING, LAYING) while carrying a waist-mounted smartphone (Samsung Galaxy S II). Data was recorded at 50Hz using the device's accelerometer and gyroscope. The dataset is manually labeled via video recordings and split into training (70%) and test (30%) sets. 

Please note that the data in the [Human Activity Recognition with Smartphones Dataset](https://www.kaggle.com/datasets/uciml/human-activity-recognition-with-smartphones) is not raw data, but preprocessed feature vectors with a length of 561. The class distribution in the training set is as follows:

![](https://dl.espressif.com/AE/esp-iot-solution/HAR_train_distribution.png)

In this example, we demonstrate how to train the model, quantize it, and deploy it to the ESP32 platform.

## How to deploy model

### Build and train model

This example uses [PyTorch](https://pytorch.org/) to build a model, and the model architecture is:

```python
class HARModel(nn.Module):
    def __init__(self):
        super(HARModel, self).__init__()

        self.model = nn.Sequential(
            nn.Linear(561, 256),
            nn.ReLU(),
            nn.Linear(256, 128),
            nn.ReLU(),
            nn.Linear(128, 6)
        )

    def forward(self, x):
        output = self.model(x)
        return output
```
> **Note:** In the model design, 561 represents the number of input features, while 6 denotes the number of output classes, corresponding to the different activity categories in the human activity recognition task.

Next, the data needs to be preprocessed:

```python
mean = X_train.mean(dim=0, keepdim=True)
std = X_train.std(dim=0, keepdim=True)
X_train = (X_train - mean) / std
X_test = (X_test - mean) / std

train_dataset = TensorDataset(X_train, y_train)
test_dataset = TensorDataset(X_test, y_test)

train_loader = DataLoader(train_dataset, batch_size=64, shuffle=True)
test_loader = DataLoader(test_dataset, batch_size=64, shuffle=False)
```

After that, we train the model using the Adam optimizer and the cross-entropy loss function:

```python
model = HARModel().to(DEVICE)

criterion = torch.nn.CrossEntropyLoss()
optimizer = torch.optim.Adam(model.parameters(), lr=0.001)

num_epochs = 100

for epoch in range(num_epochs):
    model.train()
    running_loss = 0.0
    for inputs, labels in train_loader:
        inputs, labels = inputs.to(DEVICE), labels.to(DEVICE)
        outputs = model(inputs)
        loss = criterion(outputs, labels)
        optimizer.zero_grad()
        loss.backward()
        optimizer.step()

        running_loss += loss.item()

    avg_train_loss = running_loss / len(train_loader)
    print(f"Epoch [{epoch + 1}/{num_epochs}], Loss: {avg_train_loss:.4f}")

    model.eval()
    correct = 0
    total = 0
    with torch.no_grad():
        for test_inputs, test_labels in test_loader:
            test_inputs, test_labels = test_inputs.to(DEVICE), test_labels.to(DEVICE)
            outputs = model(test_inputs)
            _, predicted = torch.max(outputs.data, 1)
            total += test_labels.size(0)
            correct += (predicted == test_labels).sum().item()

    accuracy = 100 * correct / total
    print(f"Accuracy of the model on the validation data: {accuracy:.2f}%")
```

### Model Quantization and Deployment

In order to convert the model to the format required by ``esp-dl``, the [esp-ppq](https://github.com/espressif/esp-ppq) conversion tool needs to be installed:

```shell
pip uninstall ppq
pip install git+https://github.com/espressif/esp-ppq.git
```

Next, use the ``esp-ppq`` tool to quantize and convert the model; the conversion process can be referenced as follows:

```python
BATCH_SIZE = 1
DEVICE = "cpu"
TARGET = "esp32p4"
NUM_OF_BITS = 8
input_shape = [561]
ESPDL_MODLE_PATH = "./p4/har.espdl"

class HARFeature(Dataset):
    def __init__(self, features):
        self.features = features

    def __len__(self):
        return len(self.features)

    def __getitem__(self, idx):
        return self.features[idx]

def collate_fn2(batch):
    return batch.to(DEVICE)

if __name__ == '__main__':
    x_test, y_test = load_and_preprocess_data(
        "../dataset/train.csv",
        "../dataset/test.csv")

    test_input = x_test[0]
    test_input = test_input.unsqueeze(0)

    model = HARModel()
    model.load_state_dict(torch.load("final_model.pth", map_location="cpu"))
    model.eval()

    har_dataset = HARFeature(x_test)
    har_dataloader = DataLoader(har_dataset, batch_size=BATCH_SIZE, shuffle=False)

    quant_ppq_graph = espdl_quantize_torch(
        model=model,
        espdl_export_file=ESPDL_MODLE_PATH,
        calib_dataloader=har_dataloader,
        calib_steps=8,
        input_shape=[1] + input_shape,
        inputs=[test_input],
        target=TARGET,
        num_of_bits=NUM_OF_BITS,
        collate_fn=collate_fn2,
        device=DEVICE,
        error_report=True,
        skip_export=False,
        export_test_values=True,
        verbose=1,
        dispatching_override=None
    )
```

During the conversion process, we applied 8-bit quantization to reduce the model's storage requirements and improve inference speed.

The output of the conversion process is as follows:

```shell
[INFO][ESPDL][2025-02-10 20:07:01]:  Calibration dataset samples: 2947, len(Calibrate iter): 2947
[20:07:01] PPQ Quantize Simplify Pass Running ...         Finished.
[20:07:01] PPQ Quantization Fusion Pass Running ...       Finished.
[20:07:01] PPQ Parameter Quantization Pass Running ...    Finished.
Calibration Progress(Phase 1): 100%|██████████| 8/8 [00:00<00:00, 615.37it/s]
Calibration Progress(Phase 2): 100%|██████████| 8/8 [00:00<00:00, 799.98it/s]
Finished.
[20:07:01] PPQ Passive Parameter Quantization Running ... Finished.
[20:07:01] PPQ Quantization Alignment Pass Running ...    Finished.
[INFO][ESPDL][2025-02-10 20:07:01]:  --------- Network Snapshot ---------
Num of Op:                    [5]
Num of Quantized Op:          [5]
Num of Variable:              [12]
Num of Quantized Var:         [12]
------- Quantization Snapshot ------
Num of Quant Config:          [16]
ACTIVATED:                    [7]
OVERLAPPED:                   [6]
PASSIVE:                      [3]

[INFO][ESPDL][2025-02-10 20:07:01]:  Network Quantization Finished.
Analysing Graphwise Quantization Error(Phrase 1):: 100%|██████████| 8/8 [00:00<00:00, 235.29it/s]
Analysing Graphwise Quantization Error(Phrase 2):: 100%|██████████| 8/8 [00:00<00:00, 166.67it/s]
Analysing Layerwise quantization error::   0%|          | 0/3 [00:00<?, ?it/s]Layer                | NOISE:SIGNAL POWER RATIO 
/model/model.0/Gemm: | ████████████████████ | 0.012%
/model/model.2/Gemm: | ███████              | 0.009%
/model/model.4/Gemm: |                      | 0.007%
Analysing Layerwise quantization error:: 100%|██████████| 3/3 [00:00<00:00, 88.23it/s]
Layer                | NOISE:SIGNAL POWER RATIO 
/model/model.4/Gemm: | ████████████████████ | 0.003%
/model/model.0/Gemm: | █████████████████    | 0.003%
/model/model.2/Gemm: |                      | 0.000%

Process finished with exit code 0
```

Please note that esp-ppq supports adding test data during the export of the espdl model. You can place the data in a list and pass it to the ``inputs`` parameter of ``espdl_quantize_torch``. This is important for troubleshooting model inference issues.

You can check the model's inference results in the exported info file:

```shell
test outputs value:
%11, shape: [1, 6], exponents: [-2], 
value: array([-32, -41,  82, -54, -51, -51], dtype=int8)
```


### Load model in ESP Platform

You can load models from ``rodata``, ``partition``, or ``sdcard``. In this example, the ``partition`` method is used; for more details, please refer to [how_to_load_model](https://github.com/espressif/esp-dl/blob/master/docs/en/tutorials/how_to_load_test_profile_model.rst).

Additionally, to ensure the model outputs the expected results, you need to preprocess the data following the inference process on the PC and parse the model's inference results. You can refer to the data preprocessing and post-processing steps in this example.

## Example output

In this example, we selected the 0th, 100th, and 500th samples from x_test as test data for the ESP-DL model. The corresponding ground truth labels are ``STANDING``, ``WALKING``, and ``SITTING``, respectively.

```shell
I (25) boot: ESP-IDF v5.5-dev-1610-g9cabe79385 2nd stage bootloader
I (26) boot: compile time Feb 10 2025 16:00:45
I (26) boot: Multicore bootloader
I (29) boot: chip revision: v0.2
I (30) boot: efuse block revision: v0.1
I (34) qio_mode: Enabling default flash chip QIO
I (38) boot.esp32p4: SPI Speed      : 80MHz
I (42) boot.esp32p4: SPI Mode       : QIO
I (46) boot.esp32p4: SPI Flash Size : 16MB
I (50) boot: Enabling RNG early entropy source...
I (54) boot: Partition Table:
I (57) boot: ## Label            Usage          Type ST Offset   Length
I (63) boot:  0 factory          factory app      00 00 00010000 007d0000
I (70) boot:  1 model            Unknown data     01 82 007e0000 007b7000
I (77) boot: End of partition table
I (80) esp_image: segment 0: paddr=00010020 vaddr=480f0020 size=16730h ( 91952) map
I (102) esp_image: segment 1: paddr=00026758 vaddr=30100000 size=00068h (   104) load
I (104) esp_image: segment 2: paddr=000267c8 vaddr=4ff00000 size=09850h ( 38992) load
I (114) esp_image: segment 3: paddr=00030020 vaddr=48000020 size=e16e4h (923364) map
I (257) esp_image: segment 4: paddr=0011170c vaddr=4ff09850 size=094c4h ( 38084) load
I (266) esp_image: segment 5: paddr=0011abd8 vaddr=4ff12d80 size=0337ch ( 13180) load
I (274) boot: Loaded app from partition at offset 0x10000
I (274) boot: Disabling RNG early entropy source...
I (284) hex_psram: vendor id    : 0x0d (AP)
I (284) hex_psram: Latency      : 0x01 (Fixed)
I (285) hex_psram: DriveStr.    : 0x00 (25 Ohm)
I (285) hex_psram: dev id       : 0x03 (generation 4)
I (290) hex_psram: density      : 0x07 (256 Mbit)
I (295) hex_psram: good-die     : 0x06 (Pass)
I (299) hex_psram: SRF          : 0x02 (Slow Refresh)
I (303) hex_psram: BurstType    : 0x00 ( Wrap)
I (308) hex_psram: BurstLen     : 0x03 (2048 Byte)
I (312) hex_psram: BitMode      : 0x01 (X16 Mode)
I (316) hex_psram: Readlatency  : 0x04 (14 cycles@Fixed)
I (321) hex_psram: DriveStrength: 0x00 (1/1)
I (326) MSPI DQS: tuning success, best phase id is 2
I (508) MSPI DQS: tuning success, best delayline id is 11
I esp_psram: Found 32MB PSRAM device
I esp_psram: Speed: 200MHz
I (515) mmu_psram: .rodata xip on psram
I (560) mmu_psram: .text xip on psram
I (561) hex_psram: psram CS IO is dedicated
I (561) cpu_start: Multicore app
I (1054) esp_psram: SPI SRAM memory test OK
I (1064) cpu_start: Pro cpu start user code
I (1064) cpu_start: cpu freq: 360000000 Hz
I (1064) app_init: Application information:
I (1064) app_init: Project name:     human_activity_recognition
I (1070) app_init: App version:      ca0d8a28-dirty
I (1074) app_init: Compile time:     Feb 10 2025 20:12:38
I (1079) app_init: ELF file SHA256:  257da75e2...
I (1084) app_init: ESP-IDF:          v5.5-dev-1610-g9cabe79385
I (1089) efuse_init: Min chip rev:     v0.1
I (1093) efuse_init: Max chip rev:     v1.99 
I (1097) efuse_init: Chip rev:         v0.2
I (1101) heap_init: Initializing. RAM available for dynamic allocation:
I (1108) heap_init: At 4FF192E0 len 00021CE0 (135 KiB): RAM
I (1113) heap_init: At 4FF3AFC0 len 00004BF0 (18 KiB): RAM
I (1118) heap_init: At 4FF40000 len 00040000 (256 KiB): RAM
I (1123) heap_init: At 30100068 len 00001F98 (7 KiB): TCM
I (1129) esp_psram: Adding pool of 31552K of PSRAM memory to heap allocator
I (1135) spi_flash: detected chip: generic
I (1139) spi_flash: flash io: qio
I (1143) main_task: Started on CPU0
I (1179) esp_psram: Reserving pool of 32K of internal memory for DMA/internal allocations
I (1179) main_task: Calling app_main()
I (1180) FbsLoader: The storage free size is 65536 KB
I (1184) FbsLoader: The partition size is 7900 KB
I (1189) dl::Model: model:main_graph, version:0

I (1193) dl::Model: /model/model.0/Gemm: Gemm
I (1203) dl::Model: /model/model.2/Gemm: Gemm
I (1204) dl::Model: /model/model.4/Gemm: Gemm
I (1206) MemoryManagerGreedy: Maximum memory size: 832

I (343) HAR: Test case 0: Predict result: STANDING
I (346) HAR: Test case 1: Predict result: WALKING
I (349) HAR: Test case 2: Predict result: SITTING
I (354) main_task: Returned from app_main()
```