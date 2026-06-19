import os
import json
import sqlite3
import subprocess
import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt
from matplotlib.colors import LogNorm

paths = {
    "base" : "",
    "DB" : "",
    "EXE" : "",
    "JSON" : ""
}

def GenerateData():
    conn = sqlite3.connect(paths["DB"])
    cursor = conn.cursor()
    cursor.execute("""
        CREATE TABLE IF NOT EXISTS data (
            MeanLatencyMS FLOAT,
            PacketDropRatePercentage FLOAT,
            BitsPerSecond FLOAT
        )
    """)
    conn.commit()

    with open(paths["JSON"], "r") as f:
        jsonBase = json.load(f)

    for latency in range(0, 360, 10):
        latency = max(latency, 0.5)
        for packetDropRatePercentage in range(0, 55, 5):
            cursor.execute("""
                SELECT * FROM data 
                WHERE MeanLatencyMS = ? 
                AND PacketDropRatePercentage = ?
            """,
            (latency, packetDropRatePercentage))
            if(cursor.fetchall() not in [None, []]):
                #Making sure we dont repeat
                continue
            
            sd = 0.2 * latency

            jsonBase["Network Settings"]["Mean Latency ms"] = latency
            jsonBase["Protocol Settings"]["RTT ms"] = 2 * (latency + 1.5 * sd)
            jsonBase["Protocol Settings"]["SSCT ms"] =  2 * (latency + 1.5 * sd)
            jsonBase["Protocol Settings"]["SRCT ms"] = 2 * (latency + 1.5 * sd)
            jsonBase["Network Settings"]["Latency Standard Deviation"] = sd
            jsonBase["Network Settings"]["Packet Drop Rate %"] = packetDropRatePercentage

            with open(paths["JSON"], "w") as f:
                json.dump(jsonBase, f)

            print(jsonBase)

            result = (subprocess.check_output(
                [paths["EXE"]],
                text=True))
            result = float(result.strip())

            print(result)
            
            cursor.execute("""
                INSERT INTO data(
                    MeanLatencyMS,
                    PacketDropRatePercentage,
                    BitsPerSecond
                )
                VALUES (?, ?, ?)
            """,
            (latency, packetDropRatePercentage, result))
            conn.commit()

    conn.close()

def VisualiseData():
    conn = sqlite3.connect(paths["DB"])
    df = pd.read_sql_query("SELECT * FROM data", conn) 
    df["BitsPerSecond"] = df["BitsPerSecond"] / 1000.0
    heatmapData = df.pivot( index="MeanLatencyMS", columns="PacketDropRatePercentage", values="BitsPerSecond" ) 
    annotLabels = heatmapData.map(lambda x: f"{(x):.1f}")
    plt.figure(figsize=(18,18))
    #!LOG STUFF
    sns.heatmap(
        heatmapData,
        cmap="mako",
        annot=annotLabels,
        fmt="",
        norm=LogNorm(
            vmin=heatmapData[heatmapData > 0].min().min(),  # smallest non-zero
            vmax=heatmapData.max().max()
        )
    )
    plt.xlabel("Packet Drop Rate (%)")
    plt.ylabel("Mean Latency (ms)")
    plt.title(f"Heatmap Of kbps Against Mean Latency and Packet Drop Rate %")
    #plt.show()
    plt.tight_layout()
    plt.savefig(os.path.join(paths["base"], f"kbps heatmap"), dpi=600)

VisualiseData()