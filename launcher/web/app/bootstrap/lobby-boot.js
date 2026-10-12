(function () {
  function fail() {
    if (document.documentElement.dataset.lobbyBoot === 'loading')
      document.documentElement.dataset.lobbyBoot = 'failed';
  }
  document.getElementById('lobbyModule').addEventListener('error', fail);
  setTimeout(fail, 25000);
})();
