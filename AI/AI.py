# -*- coding: utf-8 -*-
"""
UART 통신 에러 예측 모델
- 케이블 길이/Baudrate → 에러 확률 예측
- 로지스틱 회귀 사용
"""

# OK/ERR 문자열을 숫자로 변환 (ML은 숫자만 처리 가능)
def convert(status):
    if status == 'OK':
        return 0  # 정상
    if status == 'ERR':
        return 1  # 에러


import pandas as pd

# CSV 로드 (C 프로그램이 생성한 파일)
# header=None: 헤더 없는 파일이므로
df = pd.read_csv('/mnt/uart_dataset.csv', header=None)

# 컬럼명 지정 (C 프로그램의 fprintf 순서와 일치)
df.columns = ['timestamp', 'status', 'sent', 'length', 'baudrate']

# status를 숫자로 변환한 'error' 컬럼 추가
df['error'] = df['status'].apply(convert)

# 특성(X)과 타겟(y) 분리
x = df[['length', 'baudrate']]  # 입력: 케이블 길이, 통신 속도
y = df['error']                  # 출력: 에러 여부 (0 or 1)


from sklearn.model_selection import train_test_split

# 훈련 80%, 테스트 20%로 분할
# random_state=0: 재현 가능하도록 시드 고정
x_train, x_test, y_train, y_test = train_test_split(
    x, y,
    test_size=0.2,
    random_state=0
)


from sklearn.linear_model import LogisticRegression

# 로지스틱 회귀: 이진 분류에 적합, 확률 출력 가능
model = LogisticRegression()
model.fit(x_train, y_train)  # 훈련 데이터로 학습

print("훈련 끝")


def predict_error(length, baudrate):
    """특정 조건에서 에러 확률 예측 (0.0~1.0)"""
    data = [[length, baudrate]]  # 2차원 형태로 (sklearn 요구사항)
    # predict_proba()[0][1]: 첫 샘플의 class=1(에러) 확률
    return model.predict_proba(data)[0][1]


print(predict_error(10, 9600))  # 10m, 9600bps에서 에러 확률


# ===== 시각화 =====
import matplotlib.pyplot as plt

plt.figure(figsize=(7, 5))

# OK 데이터: 파란색 (alpha=0.3으로 겹침 표현)
ok = df[df['error'] == 0]
plt.scatter(ok['length'], ok['baudrate'], c='blue', alpha=0.3, label='OK')

# ERR 데이터: 빨간색
err = df[df['error'] == 1]
plt.scatter(err['length'], err['baudrate'], c='red', label='ERR')

plt.xlabel("Cable Length (m)")
plt.ylabel("Baudrate")
plt.title("Actual UART Measurements (Blue=OK, Red=ERR)")
plt.grid(True)
plt.legend()
plt.show()


# ===== 추천 시스템 =====
baud_candidates = [9600, 19200, 38400, 57600, 115200, 230400]


def recommend_baudrate(length):
    """주어진 길이에서 에러율 가장 낮은 Baudrate 찾기"""
    best_baud = None
    best_prob = 9999
    
    for b in baud_candidates:
        p = predict_error(length, b)
        if p < best_prob:
            best_prob = p
            best_baud = b
    
    return best_baud, best_prob


def recommend_length(baudrate, threshold=0.01):
    """
    주어진 Baudrate에서 안정적인 최대 케이블 길이 찾기
    threshold: 허용 에러율 (기본 1%)
    """
    best_length = 0
    
    # 0.5m ~ 30m, 0.5m 간격으로 테스트
    for i in range(1, 61):
        length = i * 0.5
        p = predict_error(length, baudrate)
        print(f"Length: {length}m, Error Prob: {p:.6f}")
        
        if p < threshold:
            best_length = length  # 아직 안정적
        else:
            break  # 임계값 초과 → 중단 (더 긴 건 더 나쁨)
    
    return best_length


def recommend(length=None, baudrate=None):
    """통합 추천 함수"""
    if length is not None:
        # 길이 → Baudrate 추천
        b, prob = recommend_baudrate(length)
        return {
            "input_length": length,
            "recommended_baudrate": b,
            "error_probability": prob
        }
    
    if baudrate is not None:
        # Baudrate → 최대 안정 길이 추천
        max_len = recommend_length(baudrate)
        return {
            "input_baudrate": baudrate,
            "max_stable_length": max_len
        }
    
    return "length 또는 baudrate 중 하나를 넣어야 합니다."


print(recommend(baudrate=230400))