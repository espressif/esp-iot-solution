# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
import numpy as np

A = np.array([[170, 250, 331, 332, 246, 247],
              [232, 227, 228, 375, 377, 291]])
B = np.array([[0.4431, 0.4431, 0.4431, 0.17, 0.17, 0.3277],
              [0.15, 0, -0.15, -0.15, 0, -0.1],
              [0.0397, 0.0397, 0.0397, 0.0397, 0.0397, 0.0397]])

A_hom = np.vstack([A, np.ones(A.shape[1])])

M = np.zeros((3, 3))

for i in range(3):
    m_i, _, _, _ = np.linalg.lstsq(A_hom.T, B[i, :], rcond=None)
    M[i, :] = m_i

print(repr(M))

flat = M.flatten()
print_str = 'panthera_set_version_matrix'
for idx, val in enumerate(flat, start=1):
    print_str += f' -{idx} {val:.6f}'
print(print_str)

for i in range(A.shape[1]):
    a = np.array([0, 0, 1])
    a[0] = A[0][i]
    a[1] = A[1][i]
    b = M @ a
    print(repr(b))
