import numpy as np
import matplotlib.pyplot as plt

# Dados experimentais
d = np.array([2.4, 3.0, 4.25, 5.5])          # distância em cm
vo3 = np.array([2.6524, 2.1507, 1.6350, 1.0578])  # tensão em V

# Variável x = 1/d^2
x = 1 / d**2

# Ajuste linear: vo3 = k1*x + k2
k1, k2 = np.polyfit(x, vo3, 1)

print(f"k1 = {k1:.4f} V·cm²")
print(f"k2 = {k2:.4f} V")

# -----------------------------
# Gráfico 1: vo3(d)
# -----------------------------
d_smooth = np.linspace(min(d), max(d), 300)
vo3_model = k1 * (1 / d_smooth**2) + k2

plt.figure(figsize=(7,5))
plt.plot(d, vo3, 'o', label='Dados experimentais')
plt.plot(d_smooth, vo3_model, '-', label=r'Ajuste: $v_{o3}=k_1\frac{1}{d^2}+k_2$')
plt.xlabel('Distância d (cm)')
plt.ylabel(r'Tensão $v_{o3}$ (V)')
plt.title(r'Gráfico de $v_{o3}(d)$')
plt.grid(True)
plt.legend()
plt.tight_layout()
plt.show()

# -----------------------------
# Gráfico 2: vo3(1/d^2)
# -----------------------------
x_smooth = np.linspace(min(x), max(x), 300)
vo3_line = k1 * x_smooth + k2

plt.figure(figsize=(7,5))
plt.plot(x, vo3, 'o', label='Dados experimentais')
plt.plot(x_smooth, vo3_line, '-', label=fr'Ajuste linear: $v_{{o3}}={k1:.3f}x+{k2:.3f}$')
plt.xlabel(r'$1/d^2$ (cm$^{-2}$)')
plt.ylabel(r'Tensão $v_{o3}$ (V)')
plt.title(r'Gráfico de $v_{o3}(1/d^2)$')
plt.grid(True)
plt.legend()
plt.tight_layout()
plt.show()