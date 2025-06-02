function showTimestamp(checkbox) {
    const spotId = checkbox.id;
    const timestampEl = document.getElementById("timestamp-" + spotId);

    if (checkbox.checked) {
        const now = new Date();
        const hours = now.getHours().toString().padStart(2, '0');
        const minutes = now.getMinutes().toString().padStart(2, '0');
        const formattedTime = `Зайнято о: ${hours}:${minutes}`;
        timestampEl.textContent = formattedTime;
        timestampEl.style.display = "block";
    } else {
        timestampEl.textContent = "";
        timestampEl.style.display = "none";
    }
}