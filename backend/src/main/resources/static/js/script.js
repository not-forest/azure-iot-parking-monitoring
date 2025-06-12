const socket = new SockJS('/ws');
const stompClient = Stomp.over(socket);
let arrivalTimes = {};

document.addEventListener("DOMContentLoaded", function () {
  updateAllSpotColors();

  const date = new Date();
  const fullDate = `Today: ${date.getDate()}/${date.getMonth() + 1}/${date.getFullYear()}`;

  document.querySelectorAll('[data-bs-toggle="popover"]').forEach(label => {
    label.setAttribute("data-bs-content", fullDate);
    new bootstrap.Popover(label);
  });
});

stompClient.connect({}, function(frame) {
  console.log('Connected: ' + frame);

  stompClient.subscribe('/topic/parking', function(message) {
    const data = JSON.parse(message.body);
    const spotId = "1A";
    const label = document.querySelector(`label[for="${spotId}"]`);
    if (!label) return;

    const eventTime = new Date(data.time);
    const timestampId = `timestamp-${spotId}`;
    const timestampDiv = document.getElementById(timestampId);

    if (data.data.is_free === false) {
      if (!arrivalTimes[spotId]) {
        arrivalTimes[spotId] = new Date(eventTime);
      }

      const formattedArrival = arrivalTimes[spotId].toLocaleString('uk-UA', {
        year: 'numeric',
        month: '2-digit',
        day: '2-digit',
        hour: '2-digit',
        minute: '2-digit',
        second: '2-digit'
      });

      label.classList.add("occupied");
      label.classList.remove("free");

      if (data.data.is_state_changed === true) {
        label.setAttribute("data-bs-content", `Arrived: ${formattedArrival}`);
      }

      if (timestampDiv) {
        timestampDiv.innerText = '';
        timestampDiv.style.display = 'none';
      }

    } else {
      let durationText = "";
      let costText = "";

      if (arrivalTimes[spotId]) {
        const start = new Date(arrivalTimes[spotId]);
        const end = new Date(eventTime);

        const ms = end - start;
        const totalSeconds = Math.floor(ms / 1000);
        const hours = Math.floor(totalSeconds / 3600);
        const minutes = Math.floor((totalSeconds % 3600) / 60);
        const seconds = totalSeconds % 60;

        const cost = seconds * 0.1 + 6 * minutes + 360 * hours;
        
        durationText = `Parking time: ${hours}h ${minutes}m ${seconds}s`;
        costText = cost > 0 ? `Payment due: $${cost.toFixed(1)}` : `Payment due: $${cost.toFixed(1)}`;

        delete arrivalTimes[spotId];
      }

      label.classList.add("free");
      label.classList.remove("occupied");
      label.setAttribute("data-bs-content", `Free spot\n${durationText}\n${costText}`);
    }

    bootstrap.Popover.getInstance(label)?.dispose();
    new bootstrap.Popover(label);
  });
});

function updateAllSpotColors() {
  const labels = document.querySelectorAll('label[for]');
  labels.forEach(label => {
    if (label.getAttribute('for') === '1A') return;
    label.classList.add("free");
    label.classList.remove("occupied");
  });
}
