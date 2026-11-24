# MP_TerP 실레이더 C 알고리즘 가상 시뮬레이션

Python으로 가상의 초음파 측정값을 생성하지만 **알고리즘은 GitHub `src/`의 실제 C 코드를 호스트 GCC로 컴파일해 실행**한다. Python으로 C 알고리즘을 다시 구현한 모형이 아니다.

포함된 C 소스: `radar_scan.c`, `radar_frame.c`, `radar_processing.c`, `distance_filter.c`, `background_filter.c`, `target_detector.c`, `target_tracker.c`. `sim/radar_bridge.c`는 Python 호출을 위한 얇은 인터페이스다.

검증 범위: 가상 서보 각도 갱신 → 가상 초음파 Trigger/Echo → 실제 C Scan FSM → 실제 C 각도별 필터/배경 보정 → 실제 C 표적 군집화/최대 5-ID 추적.

**제외 범위**: 실제 MCU GPIO/FTM/LPIT, S32DS 빌드, OLED 전송, 실제 스텝 모터의 물리적 이동, 원점 확인, `BoardSystem` 하드웨어 어댑터, 정확한 초음파 빔폭/다중반사/실시간 지터. 아래 그래프만으로 하드웨어 정확도나 ID 안정성을 입증할 수는 없다.

## 1. 실행 환경

권장: **Windows WSL2 Ubuntu** 또는 Linux/macOS. Python 3.10+와 **네이티브 GCC** 필요. S32DS에서 사용하는 `arm-none-eabi-gcc`는 Windows/Linux에서 실행하는 DLL/.so를 만들 수 없으므로 이 시뮬레이터의 빌드 컴파일러로 사용하지 않는다.

WSL Ubuntu에서:

```bash
sudo apt update
sudo apt install -y build-essential python3 python3-venv
cd /mnt/d/WorkSpace/MP_TerP   # 저장소를 둔 실제 경로로 변경
python3 -m venv .venv
source .venv/bin/activate
python sim/run.py --scenario static --sweeps 12
```

Windows 네이티브에서도 Python 3.10+와 MinGW-w64 GCC(`gcc --version` 동작)가 있다면 PowerShell에서:

```powershell
python sim/run.py --scenario static --sweeps 12
```

컴파일러를 찾을 수 없으면 WSL 방식으로 실행하는 것을 권장한다. `CC` 환경변수로 네이티브 컴파일러를 지정할 수 있다.

## 2. 시나리오와 결과

```bash
# 기본 고정 표적 2개
python sim/run.py --scenario static --sweeps 12

# 이동하는 두 표적
python sim/run.py --scenario moving --sweeps 16

# 표적 소실 후 재등장
python sim/run.py --scenario occlusion --sweeps 14

# 표적이 교차할 때 ID 스위치 관찰
python sim/run.py --scenario crossing --sweeps 18

# 동시에 5개 표적
python sim/run.py --scenario five --sweeps 10

# 배경만 있는 장면에서 거짓 검출 확인
python sim/run.py --scenario empty --sweeps 10

# 잡음과 간헐적인 Echo 실패
python sim/run.py --scenario moving --sweeps 20 --noise 40 --dropout 0.08
```

결과는 `sim/results/<시나리오>/`에 기록된다.

| 파일 | 설명 |
|---|---|
| `summary.json` | 보정 완료 여부, 검출 프레임 수, 최대 동시 추적 수 |
| `frames.csv` | 편도 스캔마다 검출 후보 수/활성 ID 수 |
| `samples.csv` | Trigger 요청마다 각도/가상 Echo 측정 거리 |
| `candidates.csv` | 실제 C Detector의 군집 결과 |
| `tracks.csv` | 실제 C Tracker의 ID·상태·각도·거리 |
| `truth.csv` | Python이 생성한 이상적인 표적 중심 위치 |

그래프가 필요하면 별도 설치:

```bash
python -m pip install -r sim/requirements.txt
python sim/run.py --scenario crossing --sweeps 18 --plot
```

`sim/results/crossing/radar_result.png`에 거리 스캔과 추적 ID/정답 각도 비교 그래프가 저장된다. `matplotlib` 없이도 CSV와 JSON은 정상 기록된다.

## 3. 자동 회귀 테스트 (직접 실행)

```bash
python sim/selftest.py
```

여섯 개 시나리오가 C 원본 알고리즘을 호출해 **배경 보정, 정지 표적, 빈 배경, 이동 표적, 가림/재검출, 최대 5개, 교차**의 기본 조건을 검사한다. ID 교차가 정확하다고 판단하는 테스트는 아니다. 테스트가 실패하면 traceback/CSV를 확인할 것. 이 테스트는 이번 추가 시점에 작성되었으며 별도의 실제 사용자 실행 로그가 확보되지 않았다.

## 4. 동작과 정확도 해석 시 주의할 점

- 첫 편도 스캔은 **표적이 없는 배경**으로 자동 보정한다.
- 배경은 기본 3500mm 거리이며, 표적은 레이더를 향한 원형 반사체로 근사한다. 실제 초음파의 지향성과 반사율 모델은 아니다.
- 디폴트 스캔: 30~150도, 5도 간격, 각도당 10ms 정착, 30ms Echo 타임아웃.
- C의 `RadarFrame`은 편도 스캔 완료에서 처리하고, 마지막 완료 프레임은 `display_frame`에 보관한다.
- 매칭은 실제 C의 최근접 각도/거리 규칙이므로 교차·가림에서 ID 교환이 발생할 수 있다.
- 호스트 실행 속도는 MCU 측정 성능이 아니다. `CycleProfiler` 등의 실제 Cortex-M4 사이클 수는 추후 보드에서 측정해야 한다.
- C 구현에 변경이 생기면 `sim/native.py`가 소스 타임스탬프를 비교해 공유 라이브러리를 다시 빌드한다. 강제 재빌드는 `--rebuild`.

## 5. 기술 스택

- Python 표준 라이브러리: `ctypes`, `argparse`, `csv`, `json`
- C11/GCC: 실제 C 레이더 모듈 컴파일
- 선택 항목: `matplotlib` (PNG 그래프)
- 결과 데이터: 재현 가능한 seed(`--seed`) 및 CSV
