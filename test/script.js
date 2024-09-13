// Перемикання між кроками
function showStep(step) {
    document.querySelectorAll('.step').forEach((el) => el.classList.remove('active'));
    document.getElementById(step).classList.add('active');
}

// Показуємо повідомлення
function showMessage(step, message, isError = false) {
    const messageElement = document.getElementById(`step${step}Message`);
    messageElement.innerText = message;
    if (isError) {
        messageElement.classList.add('error');
    } else {
        messageElement.classList.remove('error');
    }
}

// Показуємо або ховаємо спінер завантаження
function toggleLoading(show) {
    const loadingElement = document.getElementById('loading');
    loadingElement.style.display = show ? 'block' : 'none';
}

// Переходимо до кроку 2 після прошивки
document.getElementById('nextToStep2').addEventListener('click', () => {
    showStep('step2');
});

// Переходимо до кроку 3 після введення параметрів
document.getElementById('nextToStep3').addEventListener('click', () => {
    const ssid = document.getElementById('ssid').value;
    const password = document.getElementById('password').value;
    const channelKey = document.getElementById('channelKey').value;

    if (!ssid || !password || !channelKey) {
        showMessage(2, 'Будь ласка, заповніть всі поля', true);
        return;
    }

    showStep('step3');
});

// Відправка параметрів на пристрій ESP
document.getElementById('sendParams').addEventListener('click', () => {
    const ssid = document.getElementById('ssid').value;
    const password = document.getElementById('password').value;
    const channelKey = document.getElementById('channelKey').value;

    if ('serial' in navigator) {
        toggleLoading(true);
        navigator.serial.requestPort().then(port => {
            port.open({ baudRate: 115200 }).then(() => {
                const writer = port.writable.getWriter();
                const reader = port.readable.getReader();

                // Відправляємо параметри
                const data = `AT+PARAM1="${ssid}"\r\nAT+PARAM2="${password}"\r\nAT+PARAM3="${channelKey}"\r\n`;
                writer.write(new TextEncoder().encode(data));

                // Читаємо відповідь
                reader.read().then(({ value, done }) => {
                    if (!done) {
                        const response = new TextDecoder().decode(value);
                        if (response.includes("OK")) {
                            showMessage(3, "Параметри успішно відправлені на ESP!");
                        } else {
                            showMessage(3, "Помилка відправки параметрів.", true);
                        }
                        writer.releaseLock();
                        reader.releaseLock();
                        port.close();
                        toggleLoading(false);
                    }
                }).catch(error => {
                    showMessage(3, `Помилка серійного порту: ${error.message}`, true);
                    toggleLoading(false);
                });
            }).catch(error => {
                showMessage(3, `Помилка відкриття серійного порту: ${error.message}`, true);
                toggleLoading(false);
            });
        }).catch(error => {
            showMessage(3, `Помилка вибору серійного порту: ${error.message}`, true);
            toggleLoading(false);
        });
    } else {
        showMessage(3, 'Ваш браузер не підтримує Web Serial API', true);
        toggleLoading(false);
    }
});
