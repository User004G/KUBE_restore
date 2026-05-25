import matplotlib.pyplot as plt
import numpy as np

# Data
# X-Achse beginnt bei 1, wie gewünscht. Wir zeigen einen Bereich bis 3, um die Divergenz zu sehen.
x = np.linspace(1, 3, 100)

# Lineare Beziehung (Referenz)
y_linear = x

# 5 Varianten mit Exponent zwischen 1.2 und 3
exponents = np.linspace(1.2, 3, 5)

# Plot
plt.figure(figsize=(12, 8))

# Plot Linear
plt.plot(x, y_linear, label='Linear (x^1.0)', linestyle='--', color='black', linewidth=2)

# Plot Variants
colors = plt.cm.viridis(np.linspace(0, 1, len(exponents)))

for i, exp in enumerate(exponents):
    y = x ** exp
    plt.plot(x, y, label=f'Potenz (x^{exp:.2f})', color=colors[i], linewidth=2)

plt.title('Vergleich Skalierungs-Funktionen: Linear vs. Potenz-Funktionen')
plt.xlabel('Input Multiplikator (Start bei 1.0)')
plt.ylabel('Output Skalierung')
plt.grid(True, which='both', linestyle='--', alpha=0.7)
plt.legend()
plt.minorticks_on()

# Save
output_file = 'scaling_plot.png'
plt.savefig(output_file)
print(f"Plot generated: {output_file}")
