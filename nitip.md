# Q&A preparation — session "Applied Smart Systems: Environment, Agriculture, and Computer Vision"
Paper #1571339347 · answers are written to be said aloud in about 30 seconds. Lines starting "Catatan" are for you, not for the audience.

---

## Bagian A — Memahami EMA (untuk Anda)

**Satu kalimat.** Setiap titik GPS yang diterima tidak langsung dipakai. Titik yang dipakai adalah campuran: 80% titik baru + 20% titik terfilter sebelumnya.

Rumus di kode (`ALPHA = 0.8`), dihitung terpisah untuk lintang dan bujur:

    titik_terfilter = 0.8 × titik_baru + 0.2 × titik_terfilter_sebelumnya

**Contoh angka (satu sumbu, meter).** Anggap traktor bergerak lurus 5 m tiap sampel (kecepatan 1 m/s, sampel tiap 5 detik). Titik terfilter sebelumnya = 100.

| Titik baru | Perhitungan | Titik terfilter | Tertinggal dari titik baru |
|---|---|---|---|
| 105 | 0,8×105 + 0,2×100 | 104,0 | 1,0 m |
| 110 | 0,8×110 + 0,2×104 | 108,8 | 1,2 m |
| 115 | 0,8×115 + 0,2×108,8 | 113,76 | 1,24 m |

Dua hal terlihat dari contoh ini:
1. **Lonjakan kecil diredam.** Kalau satu titik tiba-tiba melenceng, jalur terfilter hanya mengikuti 80% dari lencengnya.
2. **Ada ketertinggalan (lag).** Jalur terfilter sedikit di belakang posisi sebenarnya. Pada gerak lurus stabil, ketertinggalan itu 0,25 langkah. Dengan sampel tiap 5 detik, itu setara 1,25 detik.

**Seberapa kuat redamannya?** Untuk noise acak yang tidak saling berkorelasi, variansi berkurang menjadi 0,67 kali (simpangan baku menjadi sekitar 0,82 kali). Jadi hanya sekitar 18% pengurangan simpangan baku. Ini smoothing ringan.

**Apa yang EMA *tidak* lakukan.**
- Ia tidak menghapus drift. Drift GPS adalah pergeseran lambat, bukan goyangan acak, dan EMA ringan tidak menghilangkannya.
- Yang menghentikan hitungan jarak saat traktor tidak bekerja adalah gerbang lain: status mesin, penolak lonjakan, dan penahan titik diam (di kode: pergeseran di bawah 1 m dianggap diam).
- Pemilihan α = 0,8 dilakukan lewat penalaran, bukan perbandingan empiris. Log mentah sesi uji tidak tersimpan.

**Hubungannya dengan jarak.** Jarak dijumlahkan sepanjang jalur terfilter. Tiap langkah dihitung dari titik terfilter sebelumnya ke titik terfilter baru, dan hanya dijumlahkan saat status mesin ON. Pada gerak lurus, ketertinggalan itu tidak menambah atau mengurangi total jarak, kecuali sedikit di ujung. Di tikungan, jalur terfilter cenderung memotong sudut sedikit. Itu kemungkinan penyebab sebagian dari selisih −3,86%, tetapi ini dugaan, bukan hasil pengukuran.

**Analogi untuk audiens.** Seperti mencampur cat: 80% warna baru, 20% warna lama. Warna berubah mengikuti kenyataan, tetapi sedikit lebih halus.

---

## Bagian B — Algoritma smoothing

**Q1. How does your smoothing algorithm work?**
We use an exponential moving average. Each accepted GPS fix is blended with the previous smoothed position: 80 percent the new fix and 20 percent the previous smoothed point. It damps small jumps and costs almost nothing to compute on the server.

**Q2. Why 0.8?**
It is light smoothing. For uncorrelated noise it cuts the noise variance to about two-thirds, and it adds a lag of only about 1.25 seconds at our 5-second reporting. A smaller value would smooth more but would cut corners at the field edge, which matters because we estimate area from path length. We chose it by reasoning and did not compare it empirically.
*Catatan: jangan bilang "optimal" atau "terbaik". Jangan mengaku sudah membandingkan.*

**Q3. Why not a Kalman filter or a moving average?**
A moving average needs a buffer and adds a fixed delay. A Kalman filter needs a motion model and tuning of noise parameters. We wanted a simple server-side step first. A Kalman filter on tractor GPS has been reported to reduce distance error, and comparing it with ours is our next step.

**Q4. Does the EMA remove the drift?**
No. EMA damps small random jumps. Drift is slow wandering, which light smoothing does not remove. The engine-status check and the stationary and spike checks do that job. We did not isolate how much each stage contributes.
*Catatan: ini jawaban terpenting. Naskah menyebut "most drift suppression comes from the gates"; jangan ulangi itu, karena tidak ada ablasi.*

**Q5. Does smoothing hurt accuracy at turns?**
Slightly, in principle. The smoothed path trails the real one by about a quarter of a step and tends to cut corners a little. That could explain part of the 3.86 percent shortfall, but we did not measure it.

**Q6. What are the parameters of your filter?**
Speed above 20 km/h is rejected as a spike. A fix is also rejected if HDOP is high and the jump is more than 3 meters. Fixes within 1 meter of the last accepted point are treated as standing still. Then the EMA with 0.8.
*Catatan: pakai angka 1 m (kode), bukan 2,5 m (naskah). Lihat diskusi dengan co-author.*

---

## Bagian C — Validasi dan hasil

**Q7. Why did agreement go down after filtering?**
The raw track was closer to the reference in total length but contained drift. Errors can cancel: drift adds length, corner-cutting removes it. We do not claim the filtered number is more accurate.

**Q8. Is 6.49 m a good result?**
It is the difference between our module and a smartphone, so it shows consistency between two consumer receivers, not absolute accuracy.

**Q9. How do you know the 17 m was drift?**
It was identified in the dashboard while the tractor was standing still.
*Catatan: jawab setelah Anda mengecek status mesin pada episode itu (kueri di pesan sebelumnya). Kalau mesin OFF, katakan bahwa gerbang status mesin yang mencegahnya terhitung.*

**Q10. Only one route. Can this generalize?**
No claim of generalization. It is a feasibility test: one route, one session. Validation against RTK-grade references on more routes is the next step.

**Q11. Why is your reference not survey-grade?**
Budget and scope. The Google Maps route was traced over a Strava recording, so it is not independent of smartphone GPS, and we say so in the paper.

---

## Bagian D — Pertanian terapan

**Q12. Who benefits and how?**
Owners and service providers who need to know where a tractor is and how much it worked. The expected benefit is more trustworthy distance and area for billing, but that is an expected implication, not something we validated.

**Q13. Is it really low-cost?**
We call it low-complexity. We did not publish a cost breakdown, so we do not claim a specific cost.

**Q14. Different implements, crops, and terrain?**
The area model assumes a constant implement width and one pass per meter. Lifted implements, overlap, and irregular fields break that assumption, which is why we list it as a limitation.

**Q15. Why use engine status instead of speed?**
Speed alone cannot separate a parked tractor with drifting GPS from a slow tractor working. Battery voltage is a cheap signal that tells whether the engine is running.

**Q16. Could a charger or a different battery fool the voltage check?**
Yes. An external charger could raise the voltage. A current or vibration sensor could confirm engine state; we have not tested that.

---

## Bagian E — Tema computer vision dan sistem cerdas

**Q17. Could computer vision help here?**
Possibly, but we did not test it. One use is detecting whether the implement is lowered and working, which would address our constant-width limitation. Another is combining camera data with GNSS for position. Both need validation; we have not done either.
*Catatan: jangan menyebut angka dari makalah fusi GNSS/IMU/vision yang sempat ada di draf (Yu et al.). Referensinya tidak terverifikasi dan sudah dihapus.*

**Q18. Why not use a camera or IMU instead of a voltage sensor?**
Cost and simplicity for a first version. A voltage sensor is cheap, and engine status was our main signal. A camera or IMU could add information later.

**Q19. Is this a "smart system"? Where is the intelligence?**
It is a rule-based system: engine-status detection and spatial filtering. We do not use machine learning. A learned model could come later when we have more recorded data.

**Q20. What about the environment, such as emissions or fuel?**
Not measured here. The engine-on time we record could later support fuel or emission estimates, but we did not evaluate that.

---

## Bagian F — Sistem IoT

**Q21. What happens when there is no signal?**
The device stores data locally and resends it later. We did not stress-test this in the paper.
*Catatan: perhatikan bahwa server memberi waktu berdasarkan saat paket tiba. Kalau data tersimpan dikirim ulang sekaligus, selisih waktu antar titik hampir nol dan kecepatan terhitung terlalu tinggi, sehingga titik bisa dibuang sebagai lonjakan. Cek ini sebelum Anda menyatakan bahwa store-and-forward "mencegah kehilangan data".*

**Q22. Is the data secure?**
The backend connects to the broker over TLS with certificate checking. The paper does not evaluate security.

**Q23. Power consumption and scalability?**
Not evaluated. Engine-off power saving is future work; we tested one device on one tractor.

---

## Hindari saat menjawab
- Kata "accuracy" untuk Google Maps agreement. Pakai "agreement with a reference".
- "No distortion of the tractor's actual position" (klaim di buku TA yang tidak diuji).
- "Sensor fusion" untuk sistem ini.
- "Reduces overbilling" atau "reliable" tanpa kualifikasi.
- Menyebut ambang 2,5 m.

## Sebelum presentasi
1. Putuskan dengan co-author apakah ambang 1 m dan klaim 17 m drift perlu dikoreksi ke panitia.
2. Jalankan kueri di pesan sebelumnya: status mesin saat episode drift, dan rekonstruksi jarak terfilter (harus mendekati 570 m).
3. Cek perilaku pengiriman ulang data tersimpan (Q21).
4. Latih jawaban Q2, Q4, Q7, dan Q9 lebih dulu, karena paling mungkin muncul.
