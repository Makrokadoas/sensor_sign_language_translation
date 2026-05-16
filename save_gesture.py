import serial
import csv
import os
import threading


PORT = "COM12"
BAUDRATE = 57600
##OUTPUT_DIR = "LSTM\dataset_csv\p"
OUTPUT_DIR = "p"

stop_recording = False


def serial_reader(ser, writer, filepath_info, label):
    global stop_recording

    rows_written = 0
    bad_rows = 0
    t0 = None

    param_names = [
        "flex1", "flex2", "flex3", "flex4", "flex5",
        "ax", "ay", "az",
        "gx", "gy", "gz"
    ]

    while not stop_recording:
        try:
            line = ser.readline().decode("utf-8", errors="ignore").strip()
        except Exception:
            continue

        if not line:
            continue

        if line.startswith("#"):
            continue

        # -----------------------------
        # Проверка количества запятых
        # -----------------------------
        if line.count(",") != 11:
            #print(f"[WARN] corrupted line (comma count): {line}")
            bad_rows += 1
            continue

        parts = line.split(",")

        # -----------------------------
        # Проверка количества колонок
        # -----------------------------
        if len(parts) != 12:
            print(f"[WARN] invalid column count: {len(parts)} | line: {line}")
            bad_rows += 1
            continue

        try:
            # -----------------------------
            # Проверка параметров
            # -----------------------------
            valid_row = True

            for i, name in enumerate(param_names, start=1):

                val_str = parts[i].strip()

                # фикс двойного минуса
                val_str = val_str.replace("--", "-")

                try:
                    val = float(val_str)

                except ValueError:
                    print(f"[WARN] {name} invalid value: {parts[i]}")
                    valid_row = False
                    break

                if val == 0.0:
                    print(f"[WARN] {name} = 0")
                    valid_row = False
                    break

            if not valid_row:
                bad_rows += 1
                continue

            # -----------------------------
            # Время
            # -----------------------------
            t_abs = int(parts[0])

            if t0 is None:
                t0 = t_abs

            t_rel = t_abs - t0

            row = [
                t_rel,
                parts[1],
                parts[2],
                parts[3],
                parts[4],
                parts[5],
                parts[6],
                parts[7],
                parts[8],
                parts[9],
                parts[10],
                parts[11],
                label
            ]

            writer.writerow(row)
            rows_written += 1

        except ValueError:
            bad_rows += 1
            continue

    filepath_info["rows"] = rows_written
    filepath_info["bad_rows"] = bad_rows

def main():
    global stop_recording

    os.makedirs(OUTPUT_DIR, exist_ok=True)

    print("Запись жестов")

    gesture_name = input("Имя жеста: ").strip()

    if not gesture_name:
        return

    session = input("Session номер (например 1): ").strip()

    if not session:
        session = "1"

    session = int(session)

    repeat = 1

    try:
        ser = serial.Serial(PORT, BAUDRATE, timeout=1)
    except Exception as e:
        print("Ошибка открытия порта:", e)
        return

    print("Подключено")

    try:
        while True:

            print()
            print("b — начать запись")
            print("s — остановить запись")
            print("c — сменить жест")
            print("q — выход")

            cmd = input("> ").strip().lower()

            if cmd == "q":
                break

            if cmd == "c":
                gesture_name = input("Новый жест: ").strip()
                repeat = 1
                continue

            if cmd != "b":
                continue

            filename = (
                f"{gesture_name}"
                f"_s{session:02d}"
                f"_r{repeat:03d}.csv"
            )

            filepath = os.path.join(OUTPUT_DIR, filename)

            print("Запись:", filename)

            ser.reset_input_buffer()

            stop_recording = False
            filepath_info = {"rows": 0}

            with open(filepath, "w", newline="", encoding="utf-8") as f:

                writer = csv.writer(f)

                writer.writerow([
                    "t_ms",
                    "flex1",
                    "flex2",
                    "flex3",
                    "flex4",
                    "flex5",
                    "ax",
                    "ay",
                    "az",
                    "gx",
                    "gy",
                    "gz",
                    "label"
                ])

                thread = threading.Thread(
                    target=serial_reader,
                    args=(
                        ser,
                        writer,
                        filepath_info,
                        gesture_name
                    ),
                    daemon=True
                )

                thread.start()

                while True:
                    cmd2 = input().strip().lower()

                    if cmd2 == "s":
                        stop_recording = True
                        thread.join()
                        break

            rows = filepath_info["rows"]

            bad_rows = filepath_info.get("bad_rows", 0)

            print(
                "Сохранено:",
                rows,
                "строк"
            )

            if bad_rows > 0:
                print(
                    "Пропущено плохих строк:",
                    bad_rows
                )

                if rows < 50:
                    try:
                        os.remove(filepath)
                        print("Файл удалён — слишком мало данных (< 50 строк)")
                    except Exception as e:
                        print("Ошибка удаления файла:", e)
                else:
                    repeat += 1

    finally:
        ser.close()
        print("Порт закрыт")


if __name__ == "__main__":
    main()