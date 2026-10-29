# Emotion Classification Model

- Supported target: `ESP32-P4`
- Model file: `model/p4/emotion_cls.espdl`
- Image size: `100 x 100`
- Classes: `surprise`, `fear`, `disgust`, `happiness`, `sadness`, `anger`, `neutral`

## Model Latency

| name | image size | model(us) |
|------|------------|-----------|
| emotion_cls | 100 x 100 | 136803 |

## Test App Example

Run `test_apps` with the bundled `test.jpg`:

```text
I (1785) emotion_cls: Inference result: class_id=6, class_name=neutral, score=3.880
I (1785) emotion_cls: Inference time: 136803 us (136.80 ms)
I (1795) main_task: Returned from app_main()
```
