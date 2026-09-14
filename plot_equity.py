import pandas as pd
import matplotlib.pyplot as plt


# =========================================================
# LOAD COMBINED OOS EQUITY CURVE
# =========================================================

file_path = "results/combined_oos_equity.csv"

df = pd.read_csv(file_path)

df["timestamp"] = pd.to_datetime(df["timestamp"])


# =========================================================
# CALCULATE RUNNING PEAK
# =========================================================

df["peak"] = df["equity"].cummax()


# =========================================================
# CALCULATE DRAWDOWN
# =========================================================

df["drawdown"] = df["equity"] - df["peak"]

df["drawdown_pct"] = (
    df["drawdown"] /
    df["peak"]
) * 100


# =========================================================
# FIND MAXIMUM DRAWDOWN
# =========================================================

max_drawdown = df["drawdown"].min()

max_drawdown_pct = df["drawdown_pct"].min()

max_dd_index = df["drawdown_pct"].idxmin()

max_dd_time = df.loc[
    max_dd_index,
    "timestamp"
]

max_dd_equity = df.loc[
    max_dd_index,
    "equity"
]


# =========================================================
# PRINT RESULTS
# =========================================================

print("\n========================================")
print("       EQUITY CURVE ANALYSIS")
print("========================================")

print(
    f"Starting Equity : ${df['equity'].iloc[0]:,.2f}"
)

print(
    f"Final Equity    : ${df['equity'].iloc[-1]:,.2f}"
)

print(
    f"Maximum DD      : ${max_drawdown:,.2f}"
)

print(
    f"Maximum DD %    : {max_drawdown_pct:.2f}%"
)

print(
    f"Maximum DD Time : {max_dd_time}"
)

print(
    f"Equity at Max DD: ${max_dd_equity:,.2f}"
)


# =========================================================
# EQUITY CURVE
# =========================================================

plt.figure(figsize=(12, 6))

plt.plot(
    df["timestamp"],
    df["equity"],
    label="Strategy Equity"
)

plt.plot(
    df["timestamp"],
    df["peak"],
    linestyle="--",
    label="Running Peak"
)

plt.title(
    "Quant-Engine - Combined OOS Equity Curve"
)

plt.xlabel("Time")

plt.ylabel("Portfolio Value ($)")

plt.legend()

plt.grid(True)

plt.tight_layout()

plt.savefig(
    "results/combined_oos_equity.png",
    dpi=150
)

plt.show()


# =========================================================
# DRAWDOWN CURVE
# =========================================================

plt.figure(figsize=(12, 5))

plt.plot(
    df["timestamp"],
    df["drawdown_pct"],
    label="Drawdown %"
)

plt.title(
    "Quant-Engine - OOS Drawdown"
)

plt.xlabel("Time")

plt.ylabel("Drawdown (%)")

plt.legend()

plt.grid(True)

plt.tight_layout()

plt.savefig(
    "results/combined_oos_drawdown.png",
    dpi=150
)

plt.show()


print("\nGraphs saved:")
print("results/combined_oos_equity.png")
print("results/combined_oos_drawdown.png")