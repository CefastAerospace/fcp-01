import time

from fcp_gs.verification import run_tc_latency, run_tm_loss, summarize


def test_send_tc_receives_ack(make_link):
    link = make_link()
    result = link.send_tc("ping")
    assert result["ok"] and result["status"] == "ok"
    assert result["q"] == 3 and result["attempts"] == 1


def test_send_tc_retries_with_same_seq(make_link):
    link = make_link(drop_first=True)
    result = link.send_tc("ping", timeout=0.3)
    assert result["ok"] and result["attempts"] == 2


def test_state_tracks_telemetry(make_link):
    link = make_link()
    link.send_tc("request_tm")
    time.sleep(0.2)
    state = link.snapshot()
    assert state["latest_tm"]["md"] == "bench"
    assert state["last_meta"]["rssi"] == -80
    assert state["loss"]["lost"] == 0


def test_tm_loss_report_detects_losses(make_link):
    link = make_link(skip_n={5, 6})
    report = run_tm_loss(link, count=100, idle_timeout=0.5)
    assert report["lost"] == 2
    assert report["loss_rate"] < 0.03
    assert report["passed"] is False  # 2 perdas em ~102 pacotes passam de 1%.
    assert "REPROVADO" in summarize(report)


def test_tm_loss_report_passes_without_losses(make_link):
    link = make_link()
    report = run_tm_loss(link, count=200, idle_timeout=0.5)
    assert report["passed"] and report["tm_received"] == 200
    assert report["received"] == 202  # ACK do test_tm, 200 TMs e ACK do ping de fechamento.


def test_tm_loss_counts_closing_ack_with_slow_listener(make_link):
    # Um listener lento antes do tm-loss (ex.: a interface web) não pode fazer
    # o ACK do ping de fechamento, que revela perdas no fim da rajada, ficar fora.
    link = make_link()
    link.add_listener(lambda event: time.sleep(0.05) if event.get("pkt", {}).get("t") == "ack" else None)
    report = run_tm_loss(link, count=20, idle_timeout=0.5)
    assert report["received"] == 22


def test_tm_loss_fails_on_interrupted_burst(make_link):
    link = make_link(burst_limit=50)
    report = run_tm_loss(link, count=200, idle_timeout=0.5)
    assert report["lost"] == 0  # Sem saltos no "n": a perda só aparece pelas TMs faltando.
    assert report["tm_missing"] == 150
    assert report["passed"] is False
    assert "faltando: 150" in summarize(report)


def test_tm_loss_fails_without_closing_ack(make_link):
    link = make_link(ignore_cmds={"ping"})
    report = run_tm_loss(link, count=200, idle_timeout=0.5)
    assert report["tm_received"] == 200
    assert report["closing_ack"]["status"] == "timeout"
    assert report["passed"] is False


def test_tc_latency_report(make_link):
    link = make_link()
    report = run_tc_latency(link, count=8, gap_s=0)
    assert report["passed"]
    assert report["acked"] == 8 and report["forward_ms"]["max"] == 3
    assert "APROVADO" in summarize(report)
