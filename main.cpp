#include <QtGui>
#include <QtWidgets>
#include <fluidsynth.h>
#include <assert.h>
#include <iostream>


#define MAX_CHANNELS 10


namespace {

class FluidSynth {

	class Event {
	public:
		int type = 0;
		int channel = 0;
		int key = 0;
		int velocity = 0;
		int control = 0;
		int value = 0;
		int program = 0;
		int pitch = 0;

		Event(fluid_midi_event_t * e)
		: type(fluid_midi_event_get_type(e))
		, channel(fluid_midi_event_get_channel(e))
		, key(fluid_midi_event_get_key(e))
		, velocity(fluid_midi_event_get_velocity(e))
		, control(fluid_midi_event_get_control(e))
		, value(fluid_midi_event_get_value(e))
		, program(fluid_midi_event_get_program(e))
		, pitch(fluid_midi_event_get_pitch(e))
		{
		}

		void dump() const {
			std::cout
				<< "type=" << type
				<< " channel=" << channel
				<< " key=" << key
				<< " velocity=" << velocity
				<< " control=" << control
				<< " value=" << value
				<< " program=" << program
				<< " pitch=" << pitch
				<< std::endl;
		}

		void send(fluid_synth_t * s) {
			if (type == 144) {
				if (velocity == 0) {
					fluid_synth_noteoff(s, channel, key);
				} else {
					fluid_synth_noteon(s, channel, key, velocity);
				}
			}
		}
	};

	struct ChannelData {
		QString name_;
		std::vector<Event> recordedEvents_;
		bool recording_ = false;
		bool playing_ = false;
		bool looped_ = false;
		bool enabled_ = false;
		uint32_t recordingLengthInTicks_ = 0;
	};

	class Channel {
	public:
		Channel(FluidSynth & fluid_, uint32_t number) : fluid(fluid_), number_(number), data(fluid_.channels.at(number_)) {
		}

		/*int bank() const {
		}

		void setBank() {
		}

		int program() const {
		}

		void setProgram() {
		}*/


		uint32_t number() const { return number_; }
		const QString & name() const { return data.name_; }
		void setName(QString value) { data.name_ = value; }

		bool empty() const { return data.recordedEvents_.empty(); }
		bool recorded() const { return !empty(); }

		bool recording() const { return data.recording_; }
		void setRecording(bool value) { data.recording_ = value; }
		void startRecording() { setRecording(true); }
		void stopRecording() { setRecording(false); }
		void toggleRecording() { setRecording(!recording()); }

		bool playing() const { return data.playing_; }
		void setPlaying(bool value) { data.playing_ = value; }
		void startPlaying() { setPlaying(true); }
		void stopPlaying() { setPlaying(false); }
		void togglePlaying() { setPlaying(!playing()); }

		bool enabled() const { return data.enabled_; }
		void setEnabled(bool value) { data.enabled_ = value; }
		void enable() { setEnabled(true); }
		void disable() { setEnabled(false); }
		void toggleEnabled() { setEnabled(!enabled()); }

		bool looped() const { return data.looped_; }
		void setLooped(bool value) { data.looped_ = value; }
		void toggleLooped() { setLooped(!looped()); }

		uint32_t recordingLengthInTicks() const { return data.recordingLengthInTicks_; }
		double recordingLengthInSeconds() const { return ((double)recordingLengthInTicks()) / fluid.ticksPerSecond(); }
		void setRecordingLengthInTicks(uint32_t value) { data.recordingLengthInTicks_ = value; }

		QString stateString() const {
			QString res;
			if (playing()) res += " playing";
			if (recording()) res += " recording"; else
				if (recorded()) res += " recorded";
			if (enabled()) res += " enabled";
			if (looped()) res += " looped";
			return res.trimmed();
		}

		void onMidiEvent(Event event) {
			// std::cout << __FUNCTION__ << std::endl;
			event.dump();
			if (recording()) data.recordedEvents_.push_back(event);
			//if (enabled()) {
				event.send(fluid.synth);
			/*fluid.play(event);*/ //}
		}

	private:
		FluidSynth & fluid;
		uint32_t number_;
		ChannelData & data;
	};


	static int handle_midi_event(void * data, fluid_midi_event_t* event) {
		((FluidSynth*)data)->onMidiEvent(Event(event));
		return 0;
	}

	static void sequencer_callback(unsigned int time, fluid_event_t * event, fluid_sequencer_t * seq, void * data) {
		((FluidSynth*)data)->onSequencerEvent();
	}

public:
	FluidSynth() {
		settings = new_fluid_settings();
		fluid_settings_setstr(settings, "audio.driver", "alsa");
		synth = new_fluid_synth(settings);
		midiDriver = new_fluid_midi_driver(settings, handle_midi_event, this);
		audioDriver = new_fluid_audio_driver(settings, synth);
		sequencer = new_fluid_sequencer();
		int synth_destination = fluid_sequencer_register_fluidsynth(sequencer, synth);
		int client_destination = fluid_sequencer_register_client(sequencer, "midi_looper", sequencer_callback, this);
		fluid_synth_sfload(synth, "example.sf2", 1);
	}

	~FluidSynth() {
		delete_fluid_sequencer(sequencer);
		delete_fluid_audio_driver(audioDriver);
		delete_fluid_midi_driver(midiDriver);
		delete_fluid_synth(synth);
		delete_fluid_settings(settings);
	}

	uint32_t ticksPerSecond() {
		return (uint32_t)fluid_sequencer_get_time_scale(sequencer);
	}

	uint32_t activeChannel() const {

	}

	void setActiveChannel(uint32_t channelNumber) {
		for (uint32_t i = 0; i < MAX_CHANNELS; i++) {
			if (i == channelNumber) {
				channel(i).toggleEnabled();
			} else {
				channel(i).disable();
			}
		}
	}

	Channel channel(uint32_t number) {
		assert(number < MAX_CHANNELS);
		return Channel(*this, number);
	}

	void onMidiEvent(Event event) {
		channel(event.channel).onMidiEvent(event);
	}

	void onSequencerEvent() {
		std::cout << __FUNCTION__ << std::endl;
	}

private:
	fluid_settings_t * settings;
	fluid_synth_t * synth;
	fluid_midi_driver_t * midiDriver;
	fluid_audio_driver_t * audioDriver;
	fluid_sequencer_t * sequencer;
	std::array<ChannelData, MAX_CHANNELS> channels;
};


static FluidSynth fluid;


class ChannelsModel : public QAbstractTableModel {
public:
	ChannelsModel() {
	}

	int columnCount(const QModelIndex & parent = QModelIndex()) const override {
		if (parent.isValid()) return 0;
		return 4;
	}

	int rowCount(const QModelIndex & parent = QModelIndex()) const override {
		if (parent.isValid()) return 0;
		return 10;
	}

	QVariant headerData(int section, Qt::Orientation orientation, int role) const override {
		if (role != Qt::DisplayRole) return QVariant();

		if (orientation == Qt::Horizontal) {
			static std::array<const char*, 4> headerStrings = {{ "state", "name", "length", "progress" }};
			return headerStrings.at(static_cast<size_t>(section));
		} else {
			return QString::number(section+1);
		}
	}

	QVariant data(const QModelIndex & index, int role) const override {
		if (role != Qt::DisplayRole) return QVariant();
		const auto & channel = fluid.channel(static_cast<uint32_t>(index.row()));

		switch (index.column()) {
			case 0: return channel.stateString();
			case 1: return channel.name();
			case 2: return channel.recordingLengthInSeconds();
			default: return QVariant();
		}
	}

	void updated() {
		emit dataChanged(index(0, 0), index(rowCount(), columnCount()));
	}

private:
};


class ChannelsWidget : public QTableView {
public:
	ChannelsWidget(){
		setModel(&model);
		createChortcutsForChannel(0, "1", "q", "a", "z");
		createChortcutsForChannel(1, "2", "w", "s", "x");
		createChortcutsForChannel(2, "3", "e", "d", "c");
		createChortcutsForChannel(3, "4", "r", "f", "v");
		createChortcutsForChannel(4, "5", "t", "g", "b");
		createChortcutsForChannel(5, "6", "y", "h", "n");
		createChortcutsForChannel(6, "7", "u", "j", "m");
		createChortcutsForChannel(7, "8", "i", "k", ",");
		createChortcutsForChannel(8, "9", "o", "l", ".");
		createChortcutsForChannel(9, "0", "p", ";", "/");
	}

private:
	ChannelsModel model;

	void createChortcutsForChannel(
		uint32_t channelNumber,
		const char * activateShortcut,
		const char * toggleRecordingShortcut,
		const char * toggleLoopedShortcut,
		const char * togglePlayingShortcut
	) {
		connect(new QShortcut(QKeySequence(activateShortcut), this), &QShortcut::activated,
			[this, channelNumber]() {
				fluid.setActiveChannel(channelNumber);
				model.updated();
			}
		);

		connect(new QShortcut(QKeySequence(toggleRecordingShortcut), this), &QShortcut::activated,
			[this, channelNumber]() {
				fluid.channel(channelNumber).toggleRecording();
				model.updated();
			}
		);

		connect(new QShortcut(QKeySequence(toggleLoopedShortcut), this), &QShortcut::activated,
			[this, channelNumber]() {
				fluid.channel(channelNumber).toggleLooped();
				model.updated();
			}
		);

		connect(new QShortcut(QKeySequence(togglePlayingShortcut), this), &QShortcut::activated,
			[this, channelNumber]() {
				fluid.channel(channelNumber).togglePlaying();
				model.updated();
			}
		);
	}
};

}

int main(int argc, char ** argv) {
	QApplication app(argc, argv);
	ChannelsWidget mainWindow;
	mainWindow.show();
	return app.exec();
}
