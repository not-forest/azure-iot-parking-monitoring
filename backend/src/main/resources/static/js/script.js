const socket = new SockJS('/ws');
const stompClient = Stomp.over(socket);
let arrivalTime = null;
document.addEventListener("DOMContentLoaded", function () {
  updateAllSpotColors();
});

document.addEventListener("DOMContentLoaded", function () {
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
        const label = document.querySelector('label[for="1A"]');

        if (label) {
            const eventTime = new Date(data.time);

            if (data.data.is_free === false) {
                arrivalTime = eventTime;

                const formattedArrival = eventTime.toLocaleString('uk-UA', {
                    year: 'numeric',
                    month: '2-digit',
                    day: '2-digit',
                    hour: '2-digit',
                    minute: '2-digit',
                    second: '2-digit'
                });

                label.classList.add("occupied");
                label.classList.remove("free");
                 if (data.data.is_state_changed === true){
                label.setAttribute("data-bs-content", `Arrived: ${formattedArrival}`);
                 }
            } else {
                let durationText = "";

                if (arrivalTime) {
                    const ms = eventTime - arrivalTime;
                    const totalSeconds = Math.floor(ms / 1000);
                    const hours = Math.floor(totalSeconds / 3600);
                    const minutes = Math.floor((totalSeconds % 3600) / 60);
                    const seconds = totalSeconds % 60;

                    durationText = `Parking time: ${hours}h ${minutes}m ${seconds}s`;
                    arrivalTime = null;
                }

                label.classList.add("free");
                label.classList.remove("occupied");
                label.setAttribute("data-bs-content", `Free spot, \n${durationText}`);
            }

            bootstrap.Popover.getInstance(label)?.dispose();
            new bootstrap.Popover(label);
        }
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