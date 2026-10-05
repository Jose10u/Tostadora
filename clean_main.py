import os
import shutil

main_dir = r"c:\Users\hurta\OneDrive\Documentos\embproyecto\tostadora_potencia\main"
backup_dir = r"c:\Users\hurta\OneDrive\Documentos\embproyecto\tostadora_potencia\main_backup_sources"
os.makedirs(backup_dir, exist_ok=True)

keep_files = ["main.c", "CMakeLists.txt"]

for fname in os.listdir(main_dir):
    if fname not in keep_files and os.path.isfile(os.path.join(main_dir, fname)):
        shutil.copy2(os.path.join(main_dir, fname), os.path.join(backup_dir, fname))
        os.remove(os.path.join(main_dir, fname))

print("Archivos de main limpiados y respaldados con éxito.")
