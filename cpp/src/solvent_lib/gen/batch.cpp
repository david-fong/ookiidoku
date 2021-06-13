#include <solvent_lib/gen/batch.hpp>
#include <solvent_util/str.hpp>

#include <thread>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <limits>

namespace solvent::lib::gen::batch {

	template<Order O>
	const unsigned ThreadFunc<O>::DEFAULT_NUM_THREADS = [](){
		const unsigned hwc = std::thread::hardware_concurrency();
		// NOTE: hardware_concurency is specified to be zero if unknown.
		return (hwc != 0) ? std::min(TRY_DEFAULT_NUM_EXTRA_THREADS_ + 1, hwc) : 1;
	}();


	template<Order O>
	Params Params::clean(void) noexcept {
		gen_params.template clean<O>();
		if (num_threads == 0) {
			num_threads = ThreadFunc<O>::DEFAULT_NUM_THREADS;
		}
		if (max_dead_end_sample_granularity == 0) {
			max_dead_end_sample_granularity = SharedData::SAMPLE_GRANULARITY_DEFAULT;
		}
		return *this;
	}


	template<Order O>
	void ThreadFunc<O>::operator()() {
		shared_data_mutex_.lock();
		while (get_progress() < params_.stop_after) [[likely]] {
			shared_data_mutex_.unlock(); //____________________
			const typename Generator<O>::GenResult gen_result
				= generator_.generate(params_.gen_params);
			shared_data_mutex_.lock(); //‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

			shared_data_.total_anys++;
			if (gen_result.status == ExitStatus::Ok) [[likely]] {
				shared_data_.total_oks++;

				auto& dist_summary_row = shared_data_.max_dead_end_samples[
					params_.max_dead_end_sample_granularity
					* (gen_result.most_dead_ends_seen)
					/ (params_.gen_params.max_dead_ends + 1)
				];
				dist_summary_row.marginal_oks++;
				dist_summary_row.marginal_ops += gen_result.op_count;
			}
			gen_result_consumer_(gen_result);
		}
		shared_data_mutex_.unlock();
	}


	template<Order O>
	const BatchReport batch(Params& params, callback_t<O> gen_result_consumer) {
		params.clean<O>();
		std::mutex shared_data_mutex;
		SharedData shared_data;
		shared_data.max_dead_end_samples.resize(params.max_dead_end_sample_granularity);

		std::vector<std::thread> threads;
		for (unsigned i = 0; i < params.num_threads; i++) {
			threads.push_back(std::thread(ThreadFunc<O>(
				params, shared_data, shared_data_mutex, gen_result_consumer)
			));
		}
		for (auto& thread : threads) {
			thread.join();
		}
		shared_data.time_elapsed = shared_data.timer.read_elapsed();

		shared_data.fraction_aborted = (shared_data.total_anys == 0) ? 1.0 :
			(static_cast<double>(shared_data.total_anys - shared_data.total_oks)
			/ shared_data.total_anys);
		{
			double net_ops = 0.0;
			trials_t net_oks = 0;
			for (unsigned i = 0; i < params.max_dead_end_sample_granularity; i++) {
				auto& sample = shared_data.max_dead_end_samples[i];
				sample.max_dead_ends = params.gen_params.max_dead_ends * (i+1) / params.max_dead_end_sample_granularity;
				net_ops += sample.marginal_ops;
				net_oks += sample.marginal_oks;
				sample.marginal_average_ops = sample.marginal_oks ? std::optional(sample.marginal_ops / sample.marginal_oks) : std::nullopt;
				sample.net_average_ops = net_oks ? std::optional(static_cast<double>(net_ops) / net_oks) : std::nullopt;
			}
		}{
			// Get the index of the sample representing the optimal max_dead_ends setting:
			shared_data.max_dead_end_samples_best_i = 0;
			double best_net_average_ops = std::numeric_limits<double>::max();
			for (unsigned i = 0; i < shared_data.max_dead_end_samples.size(); i++) {
				const auto& sample = shared_data.max_dead_end_samples[i];
				if (sample.net_average_ops.has_value() && (sample.net_average_ops.value() < best_net_average_ops)) {
					best_net_average_ops = sample.net_average_ops.value();
					shared_data.max_dead_end_samples_best_i = i;
				}
			}
		}
		return shared_data;
	}


	void SharedData::print(std::ostream& os, const Order O) const {
		static const std::string THROUGHPUT_BAR_STRING("-------------------------");
		static const std::string TABLE_SEPARATOR =
		"\n├─────────────┼────────────┼───────────────┼───────────────┤";
		static const std::string TABLE_HEADER =
		"\n│     max     │  marginal  │   marginal    │      net      │"
		"\n│  dead ends  │    oks     │  average ops  │  average ops  │";

		os << TABLE_SEPARATOR
			<< TABLE_HEADER
			<< TABLE_SEPARATOR
			<< std::fixed << std::setprecision(2);

		const auto& best_sample = max_dead_end_samples[max_dead_end_samples_best_i];
		for (const auto& sample : max_dead_end_samples) {

			// max_dead_ends:
			if (O <= 4) {
				os << "\n│" << std::setw(11) << sample.max_dead_ends;
			} else {
				os << "\n│" << std::setw(10) << (sample.max_dead_ends / 1'000.0) << 'K';
			}

			// marginal_oks:
			os << "  │";
			if (sample.marginal_oks == 0) { os << util::str::DIM.ON; }
			os << std::setw(10) << sample.marginal_oks;
			if (sample.marginal_oks == 0) { os << util::str::DIM.OFF; }

			// marginal_average_ops:
			os << "  │";
			if (sample.marginal_oks == 0) { os << util::str::DIM.ON; }
			os << std::setw(12);
			if (sample.marginal_average_ops.has_value()) {
				os << (sample.marginal_average_ops.value() / ((O < 5) ? 1 : 1000));
			} else {
				os << " ";
			}
			os << ((O < 5) ? ' ' : 'K');
			if (sample.marginal_oks == 0) { os << util::str::DIM.OFF; }

			// net_average_ops:
			os << "  │";
			if (sample.marginal_oks == 0) { os << util::str::DIM.ON; }
			os << std::setw(12);
			if (sample.net_average_ops.has_value()) {
				os << (sample.net_average_ops.value() / ((O < 5) ? 1 : 1000));
			} else {
				os << " ";
			}
			os << ((O < 5) ? ' ' : 'K');
			if (sample.marginal_oks == 0) { os << util::str::DIM.OFF; }

			os << "  │";

			// Print a bar to visualize throughput relative to that
			// of the best. Note visual exaggeration via exponents
			// (the exponent value was chosen by taste / visual feel)
			const unsigned bar_length = (best_sample.net_average_ops.has_value() && sample.net_average_ops.has_value())
				? (THROUGHPUT_BAR_STRING.length() * (
					1.0 / (sample.net_average_ops.value() / best_sample.net_average_ops.value())
				))
				: 0;
			if (&sample != &best_sample) os << util::str::DIM.ON;
			os << ' ' << THROUGHPUT_BAR_STRING.substr(0, bar_length);
			if (&sample != &best_sample) os << util::str::DIM.OFF;
		}
		os << TABLE_SEPARATOR;
		if (total_oks < max_dead_end_samples.size() * gen::batch::SharedData::RECOMMENDED_OKS_PER_SAMPLE) {
			os << util::str::DIM.ON << "\nexercise caution against small datasets!" << util::str::DIM.OFF << std::endl;
		}
	}


	#define SOLVENT_TEMPL_TEMPL(O_) \
		template Params Params::clean<O_>(void) noexcept; \
		template class ThreadFunc<O_>; \
		template const BatchReport batch<O_>(Params&, callback_t<O_>);
	SOLVENT_INSTANTIATE_ORDER_TEMPLATES
	#undef SOLVENT_TEMPL_TEMPL
}
namespace std {
	#define SOLVENT_TEMPL_TEMPL(O_) \
		template class function<void(typename solvent::lib::gen::Generator<O_>::GenResult const&)>;
	SOLVENT_INSTANTIATE_ORDER_TEMPLATES
	#undef SOLVENT_TEMPL_TEMPL
}